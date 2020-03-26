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
 * Demonstrates speculative execution over a single-step debug trap.
 * This is achieved by setting the trap flag in EFLAGS register. That causes to
 * trigger the trap after each instruction. We use the trap handler in the
 * tracer to move the instruction pointer over a dead code that is executed only
 * speculatively.
 * We fork the process and run the demonstration in the child, while the parent
 * takes care for setting up the trap flag in EFLAGS and moving the instruction
 * pointer over the dead code.
 **/

#include "compiler_specifics.h"

#if !SAFESIDE_LINUX
#  error Unsupported OS. Linux required.
#endif

#if !SAFESIDE_IA32 && !SAFESIDE_X64
#  error Unsupported CPU. X86/64 required.
#endif

#include <array>
#include <cstring>
#include <iostream>

#include <asm/processor-flags.h>
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

// Points to the "nop" instruction that will be recognized by the tracer which
// will move the instruction pointer to the afterspeculation label.
extern char boundary[];

static char LeakByte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(public_data);
    sidechannel.FlushOracle();

    // Synchronize with the parent. Let it setup the trap flag in child's
    // EFLAGS.
    raise(SIGSTOP);
    // This is necessary because the call to raise may be an
    // incorrectly-predicted indirect call and speculation might continue
    // past before we've even set up the trap.
    MemoryAndSpeculationBarrier();

    // Successful access of the safe offset.
    ForceRead(oracle.data() + static_cast<size_t>(data[safe_offset]));

    // NOP instruction after the boundary label. The tracer recognizes this
    // address and moves the RIP to the afterspeculation label.
    asm volatile(
        "boundary:\n"
        "nop\n");

    // Dead code begins.
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
  // Allow the parent to trace child's execution.
  int res = ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
  if (res == -1) {
    std::cerr << "PTRACE_TRACEME failed." << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << LeakByte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

void ParentProcess(pid_t child) {
  while (true) {
    int wstatus, res;
    wait(&wstatus);
    if (!WIFSTOPPED(wstatus)) {
      break;  // Unexpected wait event.
    }

    if (WSTOPSIG(wstatus) == SIGSTOP) {
      // Toggle on the trap flag in EFLAGS.
      // Documented in https://cpu.fyi/d/47e#G4.2043
      // Linux allows the modifications: https://git.io/JvCT2
      // The child stopped itself with "raise(SIGSTOP)". We have to start
      // single-stepping the child.
      user_regs_struct regs;
      // Read general purpose register values of the child.
      res = ptrace(PTRACE_GETREGS, child, nullptr, &regs);
      if (res == -1) {
        std::cerr << "PTRACE_GETREGS failed." << std::endl;
        exit(EXIT_FAILURE);
      }

      // Toggle on the trap flag in child's EFLAGS.
      regs.eflags |= X86_EFLAGS_TF;

      // Store the modified child's EFLAGS register content.
      res = ptrace(PTRACE_SETREGS, child, nullptr, &regs);
      if (res == -1) {
        std::cerr << "PTRACE_SETREGS failed." << std::endl;
        exit(EXIT_FAILURE);
      }

    } else if (WSTOPSIG(wstatus) == SIGTRAP) {
      // The child is single-stepping. Check where is the child's instruction
      // pointer. If it points to the boundary label, move it to the
      // afterspeculation label.
      user_regs_struct regs;
      // Read general purpose register values of the child.
      res = ptrace(PTRACE_GETREGS, child, nullptr, &regs);
      if (res == -1) {
        std::cerr << "PTRACE_GETREGS failed." << std::endl;
        exit(EXIT_FAILURE);
      }

      // If the child's instruction pointer points to "boundary", move it to
      // "afterspeculation" and switch off the trap flag.
#if SAFESIDE_X64
#  define IP_REG(regs) ((regs).rip)
#else
#  define IP_REG(regs) ((regs).eip)
#endif

      if (IP_REG(regs) == reinterpret_cast<size_t>(boundary)) {
        IP_REG(regs) = reinterpret_cast<size_t>(afterspeculation);

        // Toggle off the trap flag in child's EFLAGS.
        regs.eflags &= ~0x100;

        // Store the modified child's EFLAGS and RIP register content.
        res = ptrace(PTRACE_SETREGS, child, nullptr, &regs);
        if (res == -1) {
          std::cerr << "PTRACE_SETREGS failed." << std::endl;
          exit(EXIT_FAILURE);
        }
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

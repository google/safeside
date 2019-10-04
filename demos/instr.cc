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

// File containing architecturally dependent features implemented in inline
// assembler.
#include "instr.h"

#include <cstdint>

#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64) || \
    defined(_M_IX86)
#  ifdef _MSC_VER
#    include <intrin.h>
#  elif defined(__GNUC__)
#    include <x86intrin.h>
#  else
#    error Unsupported compiler.
#  endif  // _MSC_VER
#elif defined(__aarch64__)
// No headers for ARM.
#elif defined(__powerpc__)
// No headers for PowerPC.
#else
#  error Unsupported CPU.
#endif  // __i386__

// Architecturally dependent full memory fence.
static void MFence() {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64) || \
    defined(_M_IX86)
  _mm_mfence();
#elif defined(__aarch64__)
  asm volatile("dsb sy");
#elif defined(__powerpc__)
  asm volatile("sync");
#else
#  error Unsupported CPU.
#endif
}

// Architecturally dependent load memory fence.
static void LFence() {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64) || \
    defined(_M_IX86)
  _mm_lfence();
#elif defined(__aarch64__)
  asm volatile("dsb ld");
#elif defined(__powerpc__)
  asm volatile("sync");
#else
#  error Unsupported CPU.
#endif
}

// Architecturally dependent CPU clock counter.
static uint64_t RdTsc() {
  uint64_t result;
#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64) || \
    defined(_M_IX86)
  result = __rdtsc();
#elif defined(__aarch64__)
  asm volatile("mrs %0, cntvct_el0" : "=r"(result));
#elif defined(__powerpc__)
  asm volatile("mftb %0" : "=r"(result));
#else
#  error Unsupported CPU.
#endif
  return result;
}

// Forces a memory read of the byte at address p. This will result in the byte
// being loaded into cache.
void ForceRead(const void *p) {
  (void)*reinterpret_cast<const volatile char *>(p);
}

// Architecturally dependent cache flush.
void CLFlush(const void *memory) {
#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64) || \
    defined(_M_IX86)
  _mm_clflush(memory);
#elif defined(__aarch64__)
  asm volatile("dc civac, %0" ::"r"(memory) : "memory");
#elif defined(__powerpc__)
  asm volatile("dcbf 0, %0" ::"r"(memory) : "memory");
#else
#  error Unsupported CPU.
#endif
  MFence();
}

// Memory read latency measurement.
uint64_t ReadLatency(const void *memory) {
  uint64_t start = RdTsc();
  LFence();
  ForceRead(memory);
  MFence();  // Necessary for x86 MSVC.
  LFence();
  uint64_t result = RdTsc() - start;
  MFence();
  LFence();  // Necessary for x86 MSVC.
  return result;
}

#if defined(__GNUC__) && !defined(__powerpc__)
__attribute__((noinline))
void UnwindStackAndSlowlyReturnTo(const void *address) {
#if defined(__x86_64__) || defined(_M_X64)
  asm volatile(
      "addq $8, %%rsp\n"
      "popstack:\n"
      "addq $8, %%rsp\n"
      "cmpq %0, (%%rsp)\n"
      "jnz popstack\n"
      "clflush (%%rsp)\n"
      "mfence\n"
      "lfence\n"
      "ret\n"::"r"(address));
#elif defined(__i386__) || defined(_M_IX86)
  asm volatile(
      "addl $4, %%esp\n"
      "popstack:\n"
      "addl $4, %%esp\n"
      "cmpl %0, (%%esp)\n"
      "jnz popstack\n"
      "clflush (%%esp)\n"
      "mfence\n"
      "lfence\n"
      "ret\n"::"r"(address));
#elif defined(__aarch64__)
  asm volatile(
      // Unwind until the magic value and pop the magic value.
      "movz x9, 0x4567\n"
      "movk x9, 0x0123, lsl 16\n"
      "movk x9, 0xba98, lsl 32\n"
      "movk x9, 0xfedc, lsl 48\n"
      "popstack:\n"
      "ldr x10, [sp], #16\n"
      "cmp x9, x10\n"
      "bne popstack\n"
      // Push the return address on the stack.
      "str %0, [sp, #-16]!\n"
      // Pop the return address slowly from the stack and return.
      "mov x11, sp\n"
      "dc civac, x11\n"
      "dsb sy\n"
      "ldr x30, [sp], #16\n"
      "ret\n"::"r"(address));
#else
#  error Unsupported CPU.
#endif
}
#endif

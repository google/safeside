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

#if defined(__i386__) or defined(__x86_64__) or defined(_M_X64) or \
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
#if defined(__i386__) or defined(__x86_64__) or defined(_M_X64) or \
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
#if defined(__i386__) or defined(__x86_64__) or defined(_M_X64) or \
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
#if defined(__i386__) or defined(__x86_64__) or defined(_M_X64) or \
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
#if defined(__i386__) or defined(__x86_64__) or defined(_M_X64) or \
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

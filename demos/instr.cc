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
#include "utils.h"

#if SAFESIDE_X64 || SAFESIDE_IA32
#  if SAFESIDE_MSVC
#    include <intrin.h>
#  elif SAFESIDE_GNUC
#    include <x86intrin.h>
#  else
#    error Unsupported compiler.
#  endif  // SAFESIDE_MSVC
#elif SAFESIDE_ARM64
// No headers for ARM.
#elif SAFESIDE_PPC
// No headers for PowerPC.
#else
#  error Unsupported CPU.
#endif  // SAFESIDE_IA32

// Architecturally dependent full memory fence.
static void MFence() {
#if SAFESIDE_X64 || SAFESIDE_IA32
  _mm_mfence();
#elif SAFESIDE_ARM64
  asm volatile(
      "dsb sy\n"
      "isb\n");
#elif SAFESIDE_PPC
  asm volatile("sync");
#else
#  error Unsupported CPU.
#endif
}

// Architecturally dependent load memory fence.
static void LFence() {
#if SAFESIDE_X64 || SAFESIDE_IA32
  _mm_lfence();
#elif SAFESIDE_ARM64
  asm volatile(
      "dsb ld\n"
      "isb\n");
#elif SAFESIDE_PPC
  asm volatile("sync");
#else
#  error Unsupported CPU.
#endif
}

// Architecturally dependent CPU clock counter.
static uint64_t RdTsc() {
  uint64_t result;
#if SAFESIDE_X64 || SAFESIDE_IA32
  result = __rdtsc();
#elif SAFESIDE_ARM64
  asm volatile("mrs %0, cntvct_el0" : "=r"(result));
#elif SAFESIDE_PPC
  asm volatile("mftb %0" : "=r"(result));
#else
#  error Unsupported CPU.
#endif
  return result;
}

// Architecturally dependent cache flush.
void CLFlush(const void *memory) {
#if SAFESIDE_X64 || SAFESIDE_IA32
  _mm_clflush(memory);
#elif SAFESIDE_ARM64
  asm volatile("dc civac, %0" ::"r"(memory) : "memory");
#elif SAFESIDE_PPC
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

#if SAFESIDE_GNUC && !SAFESIDE_PPC
SAFESIDE_NEVER_INLINE
void UnwindStackAndSlowlyReturnTo(const void *address) {
#if SAFESIDE_X64
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
#elif SAFESIDE_IA32
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
#elif SAFESIDE_ARM64
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
      "isb\n"
      "ldr x30, [sp], #16\n"
      "ret\n"::"r"(address));
#else
#  error Unsupported CPU.
#endif
}
#endif

#if SAFESIDE_GNUC && SAFESIDE_IA32
// Returns the original value of FS and sets the new value.
int ExchangeFS(int input) {
  int output;
  asm volatile(
      "mov %%fs, %0\n"
      "mov %1, %%fs\n":"=d"(output):"a"(input):"memory");
  return output;
}

// Returns the original value of ES and sets the new value.
int ExchangeES(int input) {
  int output;
  asm volatile(
      "mov %%es, %0\n"
      "mov %1, %%es\n":"=d"(output):"a"(input):"memory");
  return output;
}
#endif

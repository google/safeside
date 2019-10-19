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

#include <cstdint>

// Page size.
#ifdef __powerpc__
constexpr uint32_t kPageSizeBytes = 65536;
#else
constexpr uint32_t kPageSizeBytes = 4096;
#endif

// Forced memory load.
void ForceRead(const void *p);

// Flushing cacheline containing given address.
void CLFlush(const void *memory);

// Measures the latency of memory read from a given address.
uint64_t ReadLatency(const void *memory);

#ifdef __GNUC__
// Unwinds the stack until the given pointer, flushes the stack pointer and
// returns.
void UnwindStackAndSlowlyReturnTo(const void *address);

#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64) || \
    defined(_M_IX86) || defined(__powerpc__)
// Label defined in inline assembly. Used to define addresses for the
// instruction pointer or program counter registers - either as return
// addresses (ret2spec) or for skipping failures in signal handlers
// (meltdown).
extern char afterspeculation[];

#elif defined(__aarch64__)
// Push callee-saved registers and return address on stack and mark it with
// magic value.
__attribute__((always_inline))
inline void BackupCalleeSavedRegsAndReturnAddress() {
  asm volatile(
      // Store the callee-saved regs.
      "stp x19, x20, [sp, #-16]!\n"
      "stp x21, x22, [sp, #-16]!\n"
      "stp x23, x24, [sp, #-16]!\n"
      "stp x25, x26, [sp, #-16]!\n"
      "stp x27, x28, [sp, #-16]!\n"
      "str x29, [sp, #-16]!\n"
      // Mark the end of the backup with magic value 0xfedcba9801234567.
      "movz x10, 0x4567\n"
      "movk x10, 0x0123, lsl 16\n"
      "movk x10, 0xba98, lsl 32\n"
      "movk x10, 0xfedc, lsl 48\n"
      "str x10, [sp, #-16]!\n");
}

__attribute__((always_inline))
inline void RestoreCalleeSavedRegs() {
  asm volatile(
      "ldr x29, [sp], #16\n"
      "ldp x27, x28, [sp], #16\n"
      "ldp x25, x26, [sp], #16\n"
      "ldp x23, x24, [sp], #16\n"
      "ldp x21, x22, [sp], #16\n"
      "ldp x19, x20, [sp], #16\n");
}

// This way we avoid the global vs. local relocation of the afterspeculation
// label addressing.
__attribute__((always_inline))
inline void JumpToAfterSpeculation() {
  asm volatile("b afterspeculation");
}
#endif

#if defined(__i386__) || defined(__x86_64__)
// Yields serializing instruction.
// Must be inlined in order to avoid mispredicted Spectre v2 jumps over it.
__attribute__((always_inline))
inline void MemoryAndSpeculationBarrier() {
  asm volatile("cpuid"::"a"(0):"ebx", "ecx", "edx", "memory");
}
#endif

#ifdef __i386__
// Returns the original value of FS and sets the new value.
int ExchangeFS(int input);
// Returns the original value of ES and sets the new value.
int ExchangeES(int input);

// Reads an offset from the FS segment.
// Must be inlined because the fault occurs inside and the stack pointer would
// be shifted.
__attribute__((always_inline))
inline unsigned int ReadUsingFS(unsigned int offset) {
  unsigned int result;

  asm volatile(
      "movzbl %%fs:(, %1, 1), %0\n"
      :"=r"(result):"r"(offset):"memory");

  return result;
}

// Reads an offset from the ES segment.
// Must be inlined because the fault occurs inside and the stack pointer would
// be shifted.
__attribute__((always_inline))
inline unsigned int ReadUsingES(unsigned int offset) {
  unsigned int result;

  asm volatile(
      "movzbl %%es:(, %1, 1), %0\n"
      :"=r"(result):"r"(offset):"memory");

  return result;
}
#endif
#endif

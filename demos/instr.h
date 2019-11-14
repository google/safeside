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
#include <cstring>

#include "compiler_specifics.h"

#if SAFESIDE_X64 || SAFESIDE_IA32
#  if SAFESIDE_MSVC
#  include <intrin.h>
#  elif SAFESIDE_GNUC
#  include <cpuid.h>
#  else
#    error Unsupported compiler.
#  endif
#endif

// Page size.
#if SAFESIDE_PPC
constexpr uint32_t kPageSizeBytes = 65536;
#else
constexpr uint32_t kPageSizeBytes = 4096;
#endif

// Flushing cacheline containing given address.
void CLFlush(const void *memory);

// Measures the latency of memory read from a given address.
uint64_t ReadLatency(const void *memory);

// Yields serializing instruction.
// Must be inlined in order to avoid to avoid misprediction that skips the
// call.
SAFESIDE_ALWAYS_INLINE
inline void MemoryAndSpeculationBarrier() {
#if SAFESIDE_X64 || SAFESIDE_IA32
#  if SAFESIDE_MSVC
  int cpuinfo[4];
  __cpuid(cpuinfo, 0);
#  elif SAFESIDE_GNUC
  int a, b, c, d;
  __cpuid(0, a, b, c, d);
#  else
#    error Unsupported compiler.
#  endif
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

SAFESIDE_ALWAYS_INLINE
inline void GenerateNop() {
#if SAFESIDE_MSVC
  __nop();
#elif SAFESIDE_GNUC
  // Memory clobber is to prevent ICC from pushing ESI onto the stack.
  // On other compilers it has no effect.
  asm volatile("nop":::"memory");
#else
#  error Unsupported compiler.
#endif
}

#if SAFESIDE_GNUC
// Unwinds the stack until the given pointer, flushes the stack pointer and
// returns.
void UnwindStackAndSlowlyReturnTo(const void *address);

#if SAFESIDE_X64 || SAFESIDE_IA32 || SAFESIDE_PPC
// Label defined in inline assembly. Used to define addresses for the
// instruction pointer or program counter registers - either as return
// addresses (ret2spec) or for skipping failures in signal handlers
// (meltdown).
extern char afterspeculation[];

#elif SAFESIDE_ARM64
// Push callee-saved registers and return address on stack and mark it with
// magic value.
SAFESIDE_ALWAYS_INLINE
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

SAFESIDE_ALWAYS_INLINE
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
SAFESIDE_ALWAYS_INLINE
inline void JumpToAfterSpeculation() {
  asm volatile("b afterspeculation");
}
#endif

#if SAFESIDE_X64 || SAFESIDE_IA32
SAFESIDE_ALWAYS_INLINE
inline void EnforceAlignment() {
#if SAFESIDE_IA32
  asm volatile(
      "pushfl\n"
      "orl $0x00040000, (%esp)\n"
      "popfl\n");
#else
  asm volatile(
      "pushfq\n"
      "orq $0x0000000000040000, (%rsp)\n"
      "popfq\n");
#endif
}

SAFESIDE_ALWAYS_INLINE
inline void UnenforceAlignment() {
#if SAFESIDE_IA32
  asm volatile(
      "pushfl\n"
      "andl $~0x00040000, (%esp)\n"
      "popfl\n");
#else
  asm volatile(
      "pushfq\n"
      "andq $~0x0000000000040000, (%rsp)\n"
      "popfq\n");
#endif
}
#endif

#if SAFESIDE_IA32
// Returns the original value of FS and sets the new value.
int ExchangeFS(int input);
// Returns the original value of ES and sets the new value.
int ExchangeES(int input);

// Performs a bound check with the bound instruction. Works only on 32-bit x86.
// Must be inlined in order to avoid mispredicted jumps over it.
SAFESIDE_ALWAYS_INLINE
inline void BoundsCheck(const char *str, size_t offset) {
  struct {
    int32_t low;
    int32_t high;
  } string_bounds;

  string_bounds.low = 0;
  string_bounds.high = strlen(str) - 1;

  // ICC and older versions of Clang have a bug in compiling the bound
  // instruction. They swap the operands when translating C++ to assembler
  // (basically changing the GNU syntax to Intel syntax) and afterwards the
  // assembler naturally crashes with:
  // Error: operand size mismatch for `bound'
  // Therefore we have to hardcode the opcode to mitigate the ICC bug.
  // The following line is the same as:
  // asm volatile("bound %%rax, (%%rdx)"
  //              ::"a"(offset), "d"(&string_bounds):"memory");
  asm volatile(".byte 0x62, 0x02"::"a"(offset), "d"(&string_bounds):"memory");
}

// Reads an offset from the FS segment.
// Must be inlined because the fault occurs inside and the stack pointer would
// be shifted.
// We fetch offset + 1 from the segment base, because the base is shifted one
// byte below to bypass 1-byte minimal segment size.
SAFESIDE_ALWAYS_INLINE
inline unsigned int ReadUsingFS(unsigned int offset) {
  unsigned int result;

  asm volatile(
      "movzbl %%fs:(, %1, 1), %0\n"
      :"=r"(result):"r"(offset + 1):"memory");

  return result;
}

// Reads an offset from the ES segment.
// Must be inlined because the fault occurs inside and the stack pointer would
// be shifted.
// We fetch offset + 1 from the segment base, because the base is shifted one
// byte below to bypass 1-byte minimal segment size.
SAFESIDE_ALWAYS_INLINE
inline unsigned int ReadUsingES(unsigned int offset) {
  unsigned int result;

  asm volatile(
      "movzbl %%es:(, %1, 1), %0\n"
      :"=r"(result):"r"(offset + 1):"memory");

  return result;
}

// Adds an offset to pointer, checks it is not overflowing using INTO and
// dereferences it.
SAFESIDE_ALWAYS_INLINE
inline void SupposedlySafeOffsetAndDereference(const char *address,
                                               unsigned int offset) {
  asm volatile(
      "addl %1, %0\n"
      "into\n"
      "movzbl (%0), %1\n"::"r"(address), "r"(offset):"cc");
}
#endif
#endif

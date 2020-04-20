/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_INSTR_H_
#define DEMOS_INSTR_H_

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

// Include architecture-specific implementations.
#if SAFESIDE_X64 || SAFESIDE_IA32
#  include "instr_x86.h"
#elif SAFESIDE_ARM64
#  include "instr_aarch64.h"
#elif SAFESIDE_PPC
#  include "instr_ppc64le.h"
#endif

// Full memory and speculation barrier, as described in docs/fencing.md.
// Implementation in instr_*.h.
void MemoryAndSpeculationBarrier();

// Flush the cache line containing the given address from all levels of the
// cache hierarchy. For split cache levels, `address` is flushed from dcache.
// Implementation in instr_*.h.
void FlushDataCacheLineNoBarrier(const void *address);

// Convenience wrapper to flush and wait.
inline void FlushDataCacheLine(void *address) {
  FlushDataCacheLineNoBarrier(address);
  MemoryAndSpeculationBarrier();
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
#endif

#if SAFESIDE_ARM64 || SAFESIDE_PPC
// Push callee-saved registers and return address on stack and mark it with
// magic value.
SAFESIDE_ALWAYS_INLINE
inline void BackupCalleeSavedRegsAndReturnAddress() {
#if SAFESIDE_ARM64
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
#elif SAFESIDE_PPC
  asm volatile(
      // Store the callee-saved regs.
      "stdu 14, -8(1)\n"
      "stdu 15, -8(1)\n"
      "stdu 16, -8(1)\n"
      "stdu 17, -8(1)\n"
      "stdu 18, -8(1)\n"
      "stdu 19, -8(1)\n"
      "stdu 20, -8(1)\n"
      "stdu 21, -8(1)\n"
      "stdu 22, -8(1)\n"
      "stdu 23, -8(1)\n"
      "stdu 24, -8(1)\n"
      "stdu 25, -8(1)\n"
      "stdu 26, -8(1)\n"
      "stdu 27, -8(1)\n"
      "stdu 28, -8(1)\n"
      "stdu 29, -8(1)\n"
      "stdu 30, -8(1)\n"
      "stdu 31, -8(1)\n"
      // Mark the end of the backup with magic value 0xfedcba9801234567.
      "addi 9, 0, 0xfffffffffffffedc\n"
      "rotldi 9, 9, 16\n"
      "addi 9, 9, 0xffffffffffffba98\n"
      "rotldi 9, 9, 16\n"
      "addi 9, 9, 0x0123\n"
      "rotldi 9, 9, 16\n"
      "addi 9, 9, 0x4568\n"
      "stdu 9, -8(1)\n");
#else
#  error Unsupported architecture.
#endif
}

SAFESIDE_ALWAYS_INLINE
inline void RestoreCalleeSavedRegs() {
#if SAFESIDE_ARM64
  asm volatile(
      "ldr x29, [sp], #16\n"
      "ldp x27, x28, [sp], #16\n"
      "ldp x25, x26, [sp], #16\n"
      "ldp x23, x24, [sp], #16\n"
      "ldp x21, x22, [sp], #16\n"
      "ldp x19, x20, [sp], #16\n");
#elif SAFESIDE_PPC
  asm volatile(
      "ldu 31, 8(1)\n"
      "ldu 30, 8(1)\n"
      "ldu 29, 8(1)\n"
      "ldu 28, 8(1)\n"
      "ldu 27, 8(1)\n"
      "ldu 26, 8(1)\n"
      "ldu 25, 8(1)\n"
      "ldu 24, 8(1)\n"
      "ldu 23, 8(1)\n"
      "ldu 22, 8(1)\n"
      "ldu 21, 8(1)\n"
      "ldu 20, 8(1)\n"
      "ldu 19, 8(1)\n"
      "ldu 18, 8(1)\n"
      "ldu 17, 8(1)\n"
      "ldu 16, 8(1)\n"
      "ldu 15, 8(1)\n"
      "ldu 14, 8(1)\n"
      // Dummy load.
      "ldu 0, 8(1)\n");
#else
#  error Unsupported architecture.
#endif
}
#endif

#if SAFESIDE_ARM64
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

#endif  // DEMOS_INSTR_H_

/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
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

#if SAFESIDE_GNUC
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
      // Having an ISB instruction on this place breaks the attack on Cavium.
      "ldr x30, [sp], #16\n"
      "ret\n"::"r"(address));
#elif SAFESIDE_PPC
  asm volatile(
      // Unwind until the magic value and pop the magic value.
      "addi 5, 0, 0xfffffffffffffedc\n"
      "rotldi 5, 5, 16\n"
      "addi 5, 5, 0xffffffffffffba98\n"
      "rotldi 5, 5, 16\n"
      "addi 5, 5, 0x0123\n"
      "rotldi 5, 5, 16\n"
      "addi 5, 5, 0x4568\n"
      "popstack:\n"
      "ldu 6, 8(1)\n"
      "cmpd 5, 6\n"
      "bf eq, popstack\n"
      // Flush the stack pointer, sync, load to link register and return.
      "dcbf 0, 1\n"
      "sync\n"
      "mtlr %0\n"
      "blr\n"::"r"(address));
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

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

// Forced memory load.
void ForceRead(const void *p);

// Flushing cacheline containing given address.
void CLFlush(const void *memory);

// Measures the latency of memory read from a given address
uint64_t ReadLatency(const void *memory);

asm(
// Assembler macros for the backup and restore of general purpose registers.
    ".macro BACKUP_REGISTERS\n"
#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64) || \
    defined(_M_IX86)
    "pushq %rax\n"
    "pushq %rbx\n"
    "pushq %rcx\n"
    "pushq %rdx\n"
    "pushq %rbp\n"
    "pushq %rsi\n"
    "pushq %rdi\n"
    "pushq %r8\n"
    "pushq %r9\n"
    "pushq %r10\n"
    "pushq %r11\n"
    "pushq %r12\n"
    "pushq %r13\n"
    "pushq %r14\n"
    "pushq %r15\n"
#elif defined(__aarch64__)
    "stp x0, x1, [sp, #-16]!\n"
    "stp x2, x3, [sp, #-16]!\n"
    "stp x4, x5, [sp, #-16]!\n"
    "stp x6, x7, [sp, #-16]!\n"
    "stp x8, x9, [sp, #-16]!\n"
    "stp x10, x11, [sp, #-16]!\n"
    "stp x12, x13, [sp, #-16]!\n"
    "stp x14, x15, [sp, #-16]!\n"
    "stp x16, x17, [sp, #-16]!\n"
    "stp x18, x19, [sp, #-16]!\n"
    "stp x20, x21, [sp, #-16]!\n"
    "stp x22, x23, [sp, #-16]!\n"
    "stp x24, x25, [sp, #-16]!\n"
    "stp x26, x27, [sp, #-16]!\n"
    "stp x28, x29, [sp, #-16]!\n"
#elif defined(__powerpc__)

#else
#  error Unsupported CPU.
#endif
    ".endm\n"

    ".macro RESTORE_REGISTERS\n"
#if defined(__i386__) || defined(__x86_64__) || defined(_M_X64) || \
    defined(_M_IX86)
    "popq %r15\n"
    "popq %r14\n"
    "popq %r13\n"
    "popq %r12\n"
    "popq %r11\n"
    "popq %r10\n"
    "popq %r9\n"
    "popq %r8\n"
    "popq %rdi\n"
    "popq %rsi\n"
    "popq %rbp\n"
    "popq %rdx\n"
    "popq %rcx\n"
    "popq %rbx\n"
    "popq %rax\n"
#elif defined(__aarch64__)
    "ldp x28, x29, [sp], #16\n"
    "ldp x26, x27, [sp], #16\n"
    "ldp x24, x25, [sp], #16\n"
    "ldp x22, x23, [sp], #16\n"
    "ldp x20, x21, [sp], #16\n"
    "ldp x18, x19, [sp], #16\n"
    "ldp x16, x17, [sp], #16\n"
    "ldp x14, x15, [sp], #16\n"
    "ldp x12, x13, [sp], #16\n"
    "ldp x10, x11, [sp], #16\n"
    "ldp x8, x9, [sp], #16\n"
    "ldp x6, x7, [sp], #16\n"
    "ldp x4, x5, [sp], #16\n"
    "ldp x2, x3, [sp], #16\n"
    "ldp x0, x1, [sp], #16\n"
#elif defined(__powerpc__)

#else
#  error Unsupported CPU.
#endif
    ".endm\n"
);

/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

.intel_syntax noprefix

.global MeasureReadLatency
// uint64_t MeasureReadLatency(const void* address);
//
// See measurereadlatency_x86_64.S for more details on what this function does,
// in particular for why MFENCEs and LFENCEs occur where they do. The only
// comments here are specific to the 32-bit implementation.
MeasureReadLatency:
  // Prologue
  push ebp
  mov ebp, esp

  // Save callee-save registers
  push ebx
  push esi
  push edi

  // ebx = address
  //
  // Read the parameter from the stack into a register now so there's no chance
  // of introducing another memory operation later.
  mov ebx, dword ptr [ebp+8]

  mfence
  lfence

  // edx:eax = <time-stamp counter>
  rdtsc

  // esi:edi = edx:eax
  mov edi, eax
  mov esi, edx

  lfence

  // Read *ebx
  mov al, byte ptr [ebx]

  lfence

  // edx:eax = <time-stamp counter>
  rdtsc

  // edx:eax -= esi:edi
  sub eax, edi
  sbb edx, esi

  // Restore callee-save registers
  pop edi
  pop esi
  pop ebx

  // Epilogue
  pop ebp

  // Return 64-bit result in edx:eax
  ret

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

.intel_syntax noprefix

.global MeasureReadLatency
// uint64_t MeasureReadLatency(const char* address);
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

; Copyright 2019 Google LLC
;
; Licensed under both the 3-Clause BSD License and the GPLv2, found in the
; LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
;
; SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0

.model flat

; Enable SSE so we can use LFENCE
.XMM

.code

; See measurereadlatency_x86.S for the commented and formatted version.
public _MeasureReadLatency
_MeasureReadLatency:
  push ebp
  mov ebp, esp
  push ebx
  push esi
  push edi
  mov ebx, dword ptr [ebp+8]
  mfence
  lfence
  rdtsc
  mov edi, eax
  mov esi, edx
  lfence
  mov al, byte ptr [ebx]
  lfence
  rdtsc
  sub eax, edi
  sbb edx, esi
  pop edi
  pop esi
  pop ebx
  pop ebp
  ret

end

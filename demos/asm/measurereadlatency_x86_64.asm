; Copyright 2019 Google LLC
;
; Licensed under both the 3-Clause BSD License and the GPLv2, found in the
; LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
;
; SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0

.code

; See measurereadlatency_x86_64.S for the commented and formatted version.
;
; One difference: the `address` argument is passed in rcx on Win64.
public MeasureReadLatency
MeasureReadLatency:
  ; rcx = address

  mfence
  lfence
  rdtsc
  shl rdx, 32
  or rax, rdx
  mov r8, rax
  lfence
  mov al, byte ptr [rcx]
  lfence
  rdtsc
  shl rdx, 32
  or rax, rdx
  sub rax, r8
  ret

end

; Copyright 2019 Google LLC
;
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at
;
;   https://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.

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

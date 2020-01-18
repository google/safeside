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

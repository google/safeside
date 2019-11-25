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

// See note in `README.md` about underscores.
#ifdef __ELF__
#  define DECORATE(f) f
#elif __APPLE__
#  define DECORATE(f) _##f
#endif

.intel_syntax noprefix

.global DECORATE(MeasureReadLatency)
// uint64_t MeasureReadLatency(const char* address);
DECORATE(MeasureReadLatency):
  // rdi = address

  // Serialize the instruction stream and finish all memory operations.
  //
  // LFENCE[1] waits for all prior instructions to complete[2] before allowing
  // any later instructions to start, but (as the name would suggest) it
  // doesn't wait for memory operations other than loads to complete. So we add
  // an MFENCE[3], which is a full memory barrier but does not serialize the
  // instruction stream, followed by an LFENCE. The LFENCE must come second so
  // that it will wait for the MFENCE to complete.
  //
  // MFENCE also waits for prior CLFLUSH and CLFLUSHOPT instructions to finish
  // before continuing.[4]
  //
  // See also: the FLUSH+RELOAD paper[5], specifically Figure 4 on page 5 and
  // the accompanying explanation.
  //
  // [1] LFENCE: https://cpu.fyi/d/484#G5.136804
  // [2] At least, on Intel processors. LFENCE is _not_ documented as
  //   serializing on AMD processors (https://cpu.fyi/d/ad5#G9.3072402), but
  //   in early 2018 AMD documented a model-specific register (MSR) that
  //   system software can set to make LFENCE serializing on basically all
  //   existing and future AMD processors. (See page 3 of
  //   https://web.archive.org/web/20180219210500/https://developer.amd.com/wp-content/resources/Managing-Speculation-on-AMD-Processors.pdf.)
  //   This MSR has been set in Linux since v4.15: https://git.io/JePgI
  // [3] MFENCE: https://cpu.fyi/d/484#G7.864843
  // [4] "Memory Ordering in P6 and More Recent Processor Families":
  //   https://cpu.fyi/d/749#G13.31870
  // [5] FLUSH+RELOAD: https://eprint.iacr.org/2013/448.pdf
  mfence
  lfence

  // edx:eax = <time-stamp counter>
  // RDTSC: https://cpu.fyi/d/484#G7.432796
  rdtsc

  // rax = edx:eax
  shl rdx, 32
  or rax, rdx

  // r8 = rax
  mov r8, rax

  // Finish reading the timestamp before starting the read.
  lfence

  // Read *rdi.
  mov al, byte ptr [rdi]

  // Finish the read before reading the timestamp again. Here, LFENCE suffices
  // because it serializes the instruction stream *and* waits for load
  // operations to complete.
  lfence

  // edx:eax = <time-stamp counter>
  rdtsc

  // rax = edx:eax
  shl rdx, 32
  or rax, rdx

  // rax -= r8
  sub rax, r8

  ret

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

.global MeasureReadLatency
// uint64_t MeasureReadLatency(const char* address);
MeasureReadLatency:
  // r3 = address

  // Serialize the instruction stream and finish all memory operations.
  //
  // SYNC[1] waits for all preceding instructions to complete before any
  // subsequent instructions are initiated. It also waits until *almost* all
  // preceding memory operations have completed, with the exception of accesses
  // caused by ICBI[2].
  //
  // To wait for these last accesses, we also issue an ISYNC[3] instruction.
  // ISYNC has the same serializing effect on the instruction stream as SYNC,
  // but doesn't enforce order of any memory accesses *except* those caused by
  // a preceding ICBI.
  //
  // Of note, Linux uses `ISYNC; SYNC` as a speculation barrier on some PowerPC
  // devices: https://git.io/Je60x
  //
  // [1] SYNC: https://cpu.fyi/d/a48#G19.1034642
  // [2] ICBI: https://cpu.fyi/d/a48#G19.102046
  // [3] ISYNC: https://cpu.fyi/d/a48#G19.1020771
  isync
  sync

  // r4 = <Time Base>
  //
  // The instruction to read the Time Base used to be "Move From Time Base"
  // (MFTB, [1]) but the Power manual now recommends "Move From Special Purpose
  // Register" (MFSPR). `MFTB n` is now a mnemonic for `MFSPR n, 268`.
  //
  // [1] MFTB: https://cpu.fyi/d/a48#G21.999352
  mfspr 4, 268

  // Finish reading Time Base before starting the read.
  //
  // We only need to serialize the instruction stream and we don't need a
  // memory barrier, so ISYNC is good enough.
  isync

  // Read *r3.
  lbz 3, 0(3)

  // Finish the read before reading Time Base again. This *does* require a
  // memory barrier.
  sync

  // r3 = <Time Base>
  mfspr 3, 268

  // r3 = r3 - r4
  sub 3, 3, 4

  // "Branch to link register", i.e. return.
  blr

/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

.global MeasureReadLatency
// uint64_t MeasureReadLatency(const void* address);
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
  // [2] ICBI: https://cpu.fyi/d/a48#G19.1020460
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

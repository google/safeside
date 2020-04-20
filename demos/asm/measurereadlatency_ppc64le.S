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

  // Full memory and speculation barrier. See docs/fencing.md for details.
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

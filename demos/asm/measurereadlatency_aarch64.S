/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

.global MeasureReadLatency
// uint64_t MeasureReadLatency(const char* address);
MeasureReadLatency:
  // x0 = address

  // Serialize the instruction stream and finish all memory operations.
  //
  // The data synchronization barrier (DSB, [1]) instruction waits for all
  // memory and cache manipulation operations to finish before completing, and
  // prevents *almost* all subsequent instructions from having any effect until
  // the DSB completes.
  //
  // One exception to DSB's serialization effects is reads of the System
  // registers that are done "without causing side-effects", which some
  // platforms consider to include reading the virtual count.
  //
  // To avoiding the timestamp read passing older instructions, we also issue
  // an Instruction Synchronization Barrier (ISB, [2]). ISB ensures that the
  // effect of "context-changing operations", for example "changes to System
  // registers", are visible to subsequent instructions.
  //
  // Linux adds an ISB before reading CNTVCT_EL0: https://git.io/Jeivz
  //
  // [1] DSB: https://cpu.fyi/d/047#G9.10258412
  // [2] ISB: https://cpu.fyi/d/047#G9.10257730
  dsb sy
  isb

  // x1 = <virtual count>
  // CNTVCT_EL0: https://cpu.fyi/d/047#G31.5432229
  mrs x1, cntvct_el0

  // Finish reading the virtual count before starting the read.
  dsb sy

  // Read *address.
  ldrb w0, [x0]

  // Finish the read before reading the virtual count again. As before, we need
  // ISB to prevent the timestamp read from issuing early.
  dsb sy
  isb

  // x2 = <virtual count>
  mrs x2, cntvct_el0

  // x0 = x2 - x1
  sub x0, x2, x1

  ret

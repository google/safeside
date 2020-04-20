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
  // x0 = address

  // Full memory and speculation barrier. See docs/fencing.md for details.
  //
  // Of special note: we need the ISB here to prevent some processors from
  // speculating ahead and reading the timestamp counter early. DSB doesn't
  // stop later instructions from "[r]eading ... System registers that are
  // directly or indirectly read without causing side-effects"[1], which seems
  // to include the virtual count.
  //
  // Linux adds an ISB before reading CNTVCT_EL0: https://git.io/Jeivz
  //
  // [1] DSB: https://cpu.fyi/d/047#G9.10258412
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

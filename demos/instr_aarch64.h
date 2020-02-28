/*
 * Copyright 2020 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_INSTR_AARCH64_H_
#define DEMOS_INSTR_AARCH64_H_

#include "compiler_specifics.h"

inline SAFESIDE_ALWAYS_INLINE void MemoryAndSpeculationBarrier() {
  // See docs/fencing.md
  asm volatile(
      "dsb sy\n"
      "isb\n"
      :
      :
      : "memory");
}

inline void FlushDataCacheLineNoBarrier(const void *address) {
  // "data cache clean and invalidate by virtual address to point of coherency"
  // https://cpu.fyi/d/047#G22.6241562
  asm volatile(
      "dc civac, %0\n"
      :
      : "r"(address)
      : "memory");
}

#endif  // DEMOS_INSTR_AARCH64_H_

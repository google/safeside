/*
 * Copyright 2020 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_INSTR_PPC64LE_H_
#define DEMOS_INSTR_PPC64LE_H_

#include "compiler_specifics.h"

inline SAFESIDE_ALWAYS_INLINE void MemoryAndSpeculationBarrier() {
  // See docs/fencing.md
  asm volatile(
      "isync\n"
      "sync\n"
      :
      :
      : "memory");
}

inline void FlushDataCacheLineNoBarrier(const void *address) {
  // "data cache block flush" with L=0 to invalidate the cache block across all
  // processors. https://cpu.fyi/d/a48#G19.1156482
  asm volatile(
      "dcbf 0, %0\n"
      :
      : "r"(address)
      : "memory");
}

#endif  // DEMOS_INSTR_PPC64LE_H_

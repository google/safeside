/*
 * Copyright 2020 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_INSTR_X86_H_
#define DEMOS_INSTR_X86_H_

#include "compiler_specifics.h"

#if SAFESIDE_MSVC
#  include <intrin.h>
#else
#  include <x86intrin.h>
#endif

// Implementations should stick to x86 intrinsics. Inline assembly has wildly
// different syntax in GCC/Clang and MSVC/Win32, and isn't supported at all in
// MSVC/x64.

inline SAFESIDE_ALWAYS_INLINE void MemoryAndSpeculationBarrier() {
  // See docs/fencing.md
  _mm_mfence();
  _mm_lfence();
}

inline void FlushDataCacheLineNoBarrier(const void *address) {
  _mm_clflush(address);
}

#endif  // DEMOS_INSTR_X86_H_

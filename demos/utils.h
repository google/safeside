/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_UTILS_H_
#define DEMOS_UTILS_H_

#include "compiler_specifics.h"

// Forced memory load. Loads the memory into cache. Used during both real and
// speculative execution to create a microarchitectural side effect in the
// cache. Also used for latency measurement in the FLUSH+RELOAD technique.
// Should be inlined to minimize the speculation window.
inline SAFESIDE_ALWAYS_INLINE
void ForceRead(const void *p) {
  (void)*reinterpret_cast<const volatile char *>(p);
}

// Flush a memory interval from cache. Used to induce speculative execution on
// flushed values until they are fetched back to the cache.
void FlushFromCache(const void *start, const void *end);
#if SAFESIDE_LINUX
void PinToTheFirstCore();
#endif
#endif  // DEMOS_UTILS_H
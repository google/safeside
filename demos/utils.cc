/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include <cstddef>

#include "instr.h"
#include "utils.h"

constexpr size_t kCacheLineSize = 64;

// Flush a memory interval from cache. Used to induce speculative execution on
// flushed values until they are fetched back to the cache.
void FlushFromCache(const char *start, const char *end) {
  // Start on the first byte and continue in kCacheLineSize steps.
  for (const char *ptr = start; ptr < end; ptr += kCacheLineSize) {
    CLFlush(ptr);
  }
  // Flush explicitly the last byte.
  CLFlush(end - 1);
}

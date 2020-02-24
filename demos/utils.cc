/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include "utils.h"

#include <cstddef>

#include "instr.h"

namespace {

constexpr size_t kCacheLineSize = 64;

// Returns the address of the first byte of the cache line *after* the one on
// which `addr` falls.
const void* StartOfNextCacheLine(const void* addr) {
  auto addr_n = reinterpret_cast<uintptr_t>(addr);

  // Create an address on the next cache line, then mask it to round it down to
  // cache line alignment.
  auto next_n = (addr_n + kCacheLineSize) & ~(kCacheLineSize - 1);
  return reinterpret_cast<const void*>(next_n);
}

}  // namespace

void FlushFromCache(const void *begin, const void *end) {
  for (; begin < end; begin = StartOfNextCacheLine(begin)) {
    CLFlush(begin);
  }
}

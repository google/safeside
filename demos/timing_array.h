/*
 * Copyright 2020 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_TIMING_ARRAY_H_
#define DEMOS_TIMING_ARRAY_H_

#include <array>
#include <cstddef>
#include <vector>

#include "arch_constants.h"

// TimingArray is an indexable container that makes it easy to induce and
// measure cache timing side-channels to leak the value of a single byte.
//
// TimingArray goes to some trouble to make sure that element accesses do not
// interfere with the cache presence of other elements.
//
// Specifically:
//   - Each element is on its own physical page of memory, which usually
//     prevents interference from hardware prefetchers.[1]
//   - Elements' order in memory is different than their index order, which
//     further frustrates hardware prefetchers by avoiding memory accesses at
//     any repeated stride.
//   - Elements are distributed across cache sets -- as opposed to e.g. always
//     being at offset 0 within a page -- which reduces contention within cache
//     sets, optimizing the use of L1 and L2 caches and therefore improving
//     side-channel signal by increasing the timing difference between cached
//     and uncached accesses.
//
// TimingArray also includes convenience functions for cache manipulation and
// timing measurement.
//
// Example use:
//
//     TimingArray ta;
//     int i = -1;
//
//     // Loop until we're sure we saw an element come from cache
//     while (i == -1) {
//       ta.FlushFromCache();
//       std::cout << ta[4] << std::endl;
//       i = ta.FindFirstCachedElementIndex();
//     }
//     std::cout << "item " << i << " was in cache" << std::endl;
//
// [1] See e.g. Intel's documentation at https://cpu.fyi/d/83c#G3.1121453,
//   which says data is only prefetched if it is on the "same 4K byte page".

class TimingArray {
 public:
  // ValueType is an alias for the element type of the array. Someday we might
  // want to make TimingArray a template class and take this as a parameter,
  // but for now the flexibility isn't worth the extra hassle.
  using ValueType = int;

  // This could likewise be a parameter and, likewise, isn't. Making the array
  // smaller doesn't improve the bandwidth of the side-channel, but trying to
  // leak more than a byte at a time significantly increases noise due to
  // greater cache contention.
  const size_t kElements = 256;

  TimingArray();

  TimingArray(TimingArray&) = delete;
  TimingArray& operator=(TimingArray&) = delete;

  ValueType& operator[](size_t i) { return ElementAt(i); }

  // We intentionally omit the "const" accessor:
  //    const ValueType& operator[](size_t i) const { ... }
  //
  // At the level of abstraction we care about, accessing an element at all
  // (even to read it) is not "morally const" since it mutates cache state.

  size_t size() const { return pages_.size(); }

  // Flushes all elements of the array from the cache.
  void FlushFromCache();

  // Reads elements of the array in index order, starting with index 0, and
  // looks for the first read that was fast enough to have been served by the
  // cache.
  //
  // Returns the index of the first "fast" element, or -1 if no element was
  // obviously read from the cache.
  //
  // This function uses a heuristic that errs on the side of false *negatives*,
  // so it is common to use it in a loop. Of course, reading the elements to
  // measure the time it takes brings those elements into cache, so the loop
  // must include a cache flush and re-attempt the side-channel leak.
  int FindFirstCachedElementIndex();

  // Just like `FindFirstCachedElementIndex`, except it begins right *after*
  // the index `start` and wraps around to try all array elements. That is, the
  // first element read is `(start+1) % size` and the last element read before
  // returning -1 is `start`.
  int FindFirstCachedElementIndexAfter(int start);

 private:
  // This is mostly to avoid needing `(*this)[n]` everywhere.
  ValueType& ElementAt(size_t i);

  // Build up some structs with `alignas` so we can represent:
  //   - 256 memory pages
  //   - each page split into cache lines
  //   - exactly one data element per cache line

  struct alignas(kCacheLineBytes) CacheLine {
    ValueType value;
  };

  // Just documenting: `sizeof()` _does_ include alignment padding.
  static_assert(sizeof(CacheLine) == kCacheLineBytes, "");

  struct alignas(kPageBytes) Page {
    std::array<CacheLine, kPageBytes / sizeof(CacheLine)> cache_lines;

    // Double-check our math.
    static_assert(sizeof(cache_lines) == kPageBytes, "");
  };

  // Use `vector` here instead of `array`. Otherwise, on platforms like PowerPC
  // where pages can be up to 64K, a TimingArray object becomes too large to
  // put on the stack.
  std::vector<Page> pages_{kElements};
};

#endif  // DEMOS_TIMING_ARRAY_H_

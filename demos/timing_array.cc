/*
 * Copyright 2020 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include "timing_array.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "asm/measurereadlatency.h"
#include "instr.h"
#include "utils.h"

namespace {

// Determines a threshold value (as returned from MeasureReadLatency) below
// which it is very likely the value read came from cache rather than from
// main memory.
//
// There are a *lot* of potential approaches for finding the threshold value,
// all involving some ad-hoc statistical methods.
//
// To find the threshold, we:
//   1. Create a timing array and bring all of its elements into cache.
//   2. Read all of the elements and remember the longest it took to read any
//      one element.
//   3. Repeat (1) and (2) until we've collected 1000 data points.
//   4. Sort the data points and return the 10th percentile value.
//
// TODO(mmdriley): explain why we use the max within a trial (cache sizes, TLB)
//
// Our approach is inspired in part by observations from "Opportunities and
// Limits of Remote Timing Attacks"[1].
//
// [1] https://www.cs.rice.edu/~dwallach/pub/crosby-timing2009.pdf
uint64_t FindCachedReadLatencyThreshold() {
  const int iterations = 1000;
  const int percentile = 10;

  TimingArray timing_array;
  std::vector<uint64_t> max_read_latencies;

  for (int n = 0; n < iterations; ++n) {
    // Bring all elements into cache.
    for (int i = 0; i < timing_array.size(); ++i) {
      ForceRead(&timing_array[i]);
    }

    // Read each element and keep track of the slowest read.
    uint64_t max_read_latency = std::numeric_limits<uint64_t>::min();
    for (int i = 0; i < timing_array.size(); ++i) {
      max_read_latency =
          std::max(max_read_latency, MeasureReadLatency(&timing_array[i]));
    }

    max_read_latencies.push_back(max_read_latency);
  }

  // Find and return the `percentile` max latency value.
  std::sort(max_read_latencies.begin(), max_read_latencies.end());
  int index = (percentile / 100.0) * (max_read_latencies.size() - 1);
  return max_read_latencies[index];
}

}  // namespace

TimingArray::TimingArray() {
  // Explicitly initialize the elements of the array.
  //
  // It's not important what value we write as long as we force *something* to
  // be written to each element. Otherwise, the backing allocation could be a
  // range of zero-fill-on-demand (ZFOD), copy-on-write pages that all start
  // off mapped to the same physical page of zeros. Since the cache on modern
  // Intel CPUs is physically tagged, some elements might map to the same cache
  // line and we wouldn't observe a timing difference between reading accessed
  // and unaccessed elements.
  for (int i = 0; i < size(); ++i) {
    ElementAt(i) = -1;
  }
}

TimingArray::ValueType& TimingArray::ElementAt(size_t i) {
  // Map this index to an element.
  //
  // We pull a few tricks here to minimize potential interference between
  // reading elements:
  //   - As described elsewhere, every element is on its own page to disable
  //     hardware prefetching on some systems.
  //   - Where that isn't enough (e.g. some AMD parts), we also use a simple
  //     "

  // 113 works great for 256
  size_t page = (i * 113 + 100) % pages_.size();
  size_t cache_line = (i % (kPageBytes / kCacheLineBytes));

  return pages_[page].cache_lines[cache_line].value;
}

void TimingArray::FlushFromCache() {
  // We only need to flush the cache lines with elements on them.
  for (int i = 0; i < size(); ++i) {
    CLFlush(&ElementAt(i));
  }
}

int TimingArray::FindFirstCachedElementIndexAfter(int start_after) {
  static uint64_t cached_read_latency_threshold =
      FindCachedReadLatencyThreshold();

  // Fail if element is out of bounds.
  if (start_after >= size()) {
    return -1;
  }

  // Start at the element after `start_after`, wrapping around until we've
  // found a cached element or tried every element.
  for (int i = 1; i <= size(); ++i) {
    int el = (start_after + i) % size();
    uint64_t read_latency = MeasureReadLatency(&ElementAt(el));
    if (read_latency <= cached_read_latency_threshold) {
      return el;
    }
  }

  // Didn't find a cached element.
  return -1;
}

int TimingArray::FindFirstCachedElementIndex() {
  // Start "after" the last element, which means start at the first.
  return FindFirstCachedElementIndexAfter(size() - 1);
}

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
#include <limits>
#include <iostream>
#include <vector>

#include "asm/measurereadlatency.h"
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
// TODO: explain why we use the max within a trial (cache sizes, TLB)
//
// Our approach is inspired in part by observations from "Opportunities and
// Limits of Remote Timing Attacks"[1].
//
// [1] https://www.cs.rice.edu/~dwallach/pub/crosby-timing2009.pdf
uint64_t FindCachedReadLatencyThreshold() {
  TimingArray timing_array;
  std::vector<uint64_t> max_read_latencies;

  for (int n = 0; n < 1000; ++n) {
    // Bring all elements into cache
    for (int i = 0; i < timing_array.size(); ++i) {
      ForceRead(&timing_array[i]);
    }

    // Read all elements and keep track of the slowest read.
    uint64_t max_read_latency = std::numeric_limits<uint64_t>::min();
    for (int i = 0; i < timing_array.size(); ++i) {
      max_read_latency =
          std::max(max_read_latency, MeasureReadLatency(&timing_array[i]));
    }

    max_read_latencies.push_back(max_read_latency);
  }

  // Return the 10th percentile max latency value.
  std::sort(max_read_latencies.begin(), max_read_latencies.end());
  return max_read_latencies[max_read_latencies.size() / 10];
}

}  // namespace

void TimingArray::FlushFromCache() {
  ::FlushFromCache(this, this + 1);
}

ssize_t TimingArray::FindFirstCachedElementIndexAfter(size_t start) {
  static uint64_t cached_read_latency_threshold =
    FindCachedReadLatencyThreshold();

  for (int i = start + 1; i != start; i = (i + 1) % size()) {
    if (MeasureReadLatency(&(*this)[i]) <= cached_read_latency_threshold) {
      return i;
    }
  }

  return -1;
}

ssize_t TimingArray::FindFirstCachedElementIndex() {
  return FindFirstCachedElementIndexAfter(0);
}

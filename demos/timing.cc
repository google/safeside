/*
 * Copyright 2020 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include "timing.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <iostream>
#include <vector>

#include "asm/measurereadlatency.h"
#include "utils.h"

namespace {

uint64_t FindCachedLatencyThreshold() {
  TimingArray timing_array;
  std::vector<uint64_t> max_latencies;

  for (int n = 0; n < 1000; ++n) {
    // Bring all elements into cache
    for (int i = 0; i < timing_array.size(); ++i) {
      ForceRead(&timing_array[i]);
    }

    // Measure the longest read
    uint64_t max_read_latency = std::numeric_limits<uint64_t>::min();
    for (int i = 0; i < timing_array.size(); ++i) {
      max_read_latency =
          std::max(max_read_latency, MeasureReadLatency(&timing_array[i]));
    }

    max_latencies.push_back(max_read_latency);
  }

  std::sort(max_latencies.begin(), max_latencies.end());

  // return p10
  uint64_t ret = max_latencies[max_latencies.size() * 8 / 10];
  // std::cerr << "cached read latency threshold is " << ret << std::endl;
  return ret;
}

}  // namespace

void TimingArray::FlushFromCache() {
  ::FlushFromCache(this, this + 1);
}

ssize_t TimingArray::FindFirstCachedElementIndexAfter(size_t start) {
  static uint64_t cached_latency_threshold = FindCachedLatencyThreshold();

  for (int i = start + 1; i != start; i = (i + 1) % size()) {
    if (MeasureReadLatency(&(*this)[i]) <= cached_latency_threshold) {
      return i;
    }
  }

  return -1;
}

ssize_t TimingArray::FindFirstCachedElementIndex() {
  return FindFirstCachedElementIndexAfter(0);
}

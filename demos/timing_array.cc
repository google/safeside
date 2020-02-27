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
#include <vector>

#include "asm/measurereadlatency.h"
#include "instr.h"
#include "utils.h"

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

  // Init the first time through, then keep for later instances.
  static uint64_t threshold = FindCachedReadLatencyThreshold();
  cached_read_latency_threshold_ = threshold;
}

void TimingArray::FlushFromCache() {
  // We only need to flush the cache lines with elements on them.
  for (int i = 0; i < size(); ++i) {
    FlushDataCacheLineNoBarrier(&ElementAt(i));
  }

  // Wait for flushes to finish.
  MemoryAndSpeculationBarrier();
}

int TimingArray::FindFirstCachedElementIndexAfter(int start_after) {
  // Fail if element is out of bounds.
  if (start_after >= size()) {
    return -1;
  }

  // Start at the element after `start_after`, wrapping around until we've
  // found a cached element or tried every element.
  for (int i = 1; i <= size(); ++i) {
    int el = (start_after + i) % size();
    uint64_t read_latency = MeasureReadLatency(&ElementAt(el));
    if (read_latency <= cached_read_latency_threshold_) {
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

// Determines a threshold value (as returned from MeasureReadLatency) at or
// below which it is very likely the value was read from the cache without
// going to main memory.
//
// There are a *lot* of potential approaches for finding such a threshold
// value. Ours is, roughly:
//   1. Read all the elements of a TimingArray into cache.
//   2. Read all elements again, in the same order, measuring how long each
//      read takes and remembering the latency of the slowest read.
//   3. Repeat (1) and (2) many times to get a lot of "slowest read from a
//      cached array" data points.
//   4. Sort those data points and take a value at a low percentile.
//
// We try to make our code to *find* the threshold as similar as possible as
// code elsewhere that *uses* it. Reading the whole array each time and taking
// the slowest read helps us account for effects that only happen when reading
// many values:
//   - TimingArray forces values onto different pages, which introduces TLB
//     pressure. After re-reading ~64 elements of a 256-element array on an
//     Intel Xeon processor, we see latency increase as we start hitting the L2
//     TLB. The 7-Zip LZMA benchmarks have some useful measurements on this:
//     https://www.7-cpu.com/cpu/Skylake.html
//   - Our first version of TimingArray didn't implement cache coloring and all
//     elements were contending for the same cache set. As a result, after
//     reading 256 values, we had evicted all but the first ~8 from the L1
//     cache. Our threshold computation took this into account. If we had just
//     looped over reading one memory value, the computed threshold would have
//     been too low to classify reads from L2 or L3 cache.
//
// Repeating the experiment many times and taking a low-percentile value helps
// us control for effects that would otherwise skew the threshold too high:
//   - A context switch might happen right before a measurement, evicting array
//     elements from the cache; or one could happen *during* a measurement,
//     adding arbitrary extra time to the observed latency.
//   - A coscheduled hyperthread might introduce cache contention, forcing some
//     reads to go to memory.
//
// Ultimately, our approach assumes:
//   - Read latencies are a right-skewed distribution, with a left boundary at
//     the fastest possible read from cache, a mode value slightly above that
//     speed-of-light value, and a long right tail of slow or interrupted
//     reads.
//   - Context switches and contention happen, but not too often.
//
// The idea of taking a low-percentile value is inspired in part by
// observations from "Opportunities and Limits of Remote Timing Attacks"[1].
//
// [1] https://www.cs.rice.edu/~dwallach/pub/crosby-timing2009.pdf
uint64_t TimingArray::FindCachedReadLatencyThreshold() {
  const int iterations = 1000;
  const int percentile = 10;

  // Accumulates the highest read latency seen in each iteration.
  std::vector<uint64_t> max_read_latencies;

  for (int n = 0; n < iterations; ++n) {
    // Bring all elements into cache.
    for (int i = 0; i < size(); ++i) {
      ForceRead(&ElementAt(i));
    }

    // Read each element and keep track of the slowest read.
    uint64_t max_read_latency = std::numeric_limits<uint64_t>::min();
    for (int i = 0; i < size(); ++i) {
      max_read_latency =
          std::max(max_read_latency, MeasureReadLatency(&ElementAt(i)));
    }

    max_read_latencies.push_back(max_read_latency);
  }

  // Find and return the `percentile` max read latency value.
  std::sort(max_read_latencies.begin(), max_read_latencies.end());
  int index = (percentile / 100.0) * (max_read_latencies.size() - 1);
  return max_read_latencies[index];
}

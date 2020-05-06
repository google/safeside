/*
 * Copyright 2020 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include "timing_array.h"

#include <stdlib.h>

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
// below which it is very likely the value was read from the cache without going
// to main memory.
//
// There are a *lot* of potential approaches for finding such a threshold value.
// Ours is, roughly:
//   1. Flush all array elements from cache.
//   2. Read all elements into cache, noting the fastest read.
//   3. Read all elements again, now from cache, noting the slowest read.
//   4. Repeat (1)-(3) many times to get a lot of "fastest uncached" and
//      "slowest cached" datapoints.
//   5. Take a high-percentile value from "slowest cached" and a low-percentile
//      value from "fastest uncached" and return a point between them.
//
// We try to make our code to *find* the threshold as similar as possible as
// code elsewhere that *uses* it. This helps account for effects that only
// happen when reading all values at once:
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
// FIXME: this is out of date
// Using values at low/high percentile instead of strict min/max helps prevent
// outliers from dominating the result. Some reasons we might see outliers:
//   - A context switch might happen right before a measurement, evicting array
//     elements from the cache; or one could happen *during* a measurement,
//     adding arbitrary extra time to the observed latency.
//   - A coscheduled hyperthread might introduce cache contention, forcing some
//     reads to go to memory.
//
// In a previous attempt, we tried computing a threshold by looking only at the
// latency of cached reads. This worked well in some cases, but failed on
// systems where the latency distribution could be locally consistent but
// variable over longer stretches of time -- for example, on laptops with
// aggressive dynamic frequency scaling. By taking into account the latency of
// uncached reads, we arrive at a threshold that is still accurate but a bit
// more forgiving of occasional variation.
uint64_t TimingArray::FindCachedReadLatencyThreshold() {
  const int iterations = 10000;
  const int percentile = 5;  // should be small

  // For testing, allow the threshold to be specified as an environment
  // variable.
  const char *threshold_from_env = getenv("CACHED_THRESHOLD");
  if (threshold_from_env) {
    return atoi(threshold_from_env);
  }

  std::vector<uint64_t> fast_uncached_times, slow_cached_times;
  for (int n = 0; n < iterations; ++n) {
    FlushFromCache();

    uint64_t fastest_uncached = std::numeric_limits<uint64_t>::max();
    for (int i = 0; i < size(); ++i) {
      fastest_uncached =
          std::min(fastest_uncached, MeasureReadLatency(&ElementAt(i)));
    }

    uint64_t slowest_cached = std::numeric_limits<uint64_t>::min();  // aka 0
    for (int i = 0; i < size(); ++i) {
      slowest_cached =
          std::max(slowest_cached, MeasureReadLatency(&ElementAt(i)));
    }

    fast_uncached_times.push_back(fastest_uncached);
    slow_cached_times.push_back(slowest_cached);
  }

  // Sample "small" cached and uncached times and return the midpoint.
  std::sort(slow_cached_times.begin(), slow_cached_times.end());
  std::sort(fast_uncached_times.begin(), fast_uncached_times.end());

  int index = (percentile / 100.0) * (iterations - 1);
  return (slow_cached_times[index] + fast_uncached_times[index]) / 2;
}

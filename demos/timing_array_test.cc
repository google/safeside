/*
 * Copyright 2020 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include "timing_array.h"

#include <iostream>

#include "instr.h"
#include "utils.h"

// Measure how often TimingArray is able to accurately determine which element
// was read into cache. Fail the test early if the *wrong* element is ever read
// (vs. the benign "no result").
int main(int argc, char* argv[]) {
  TimingArray ta;

  std::cout << "Cached read latency threshold is "
            << ta.cached_read_latency_threshold() << std::endl;

  const int attempts = 10000;
  int successes = 0;

  for (int n = 0; n < attempts; ++n) {
    // Choose a random byte and attempt to leak it through the cache timing
    // side-channel.
    int el = rand() & 0xff;
    ta.FlushFromCache();
    ForceRead(&ta[el]);

    int found = ta.FindFirstCachedElementIndex();
    if (found == el) {
      ++successes;
    } else if (found != -1) {
      std::cout << "False positive. Found " << found
                << " instead of " << el
                << std::endl;
      exit(1);
    }
  }

  std::cout << "Found cached element on the first try "
            << successes << " of " << attempts << " times." << std::endl;

  // Expect a high percentage of attempts to succeed.
  bool pass = successes > (attempts * 0.85);
  return !pass;
}

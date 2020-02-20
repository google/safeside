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

int main(int argc, char* argv[]) {
  TimingArray ta;
  int j = 0;
  for (int n = 0; n < 10000; ++n) {
    // std::cout << n << std::endl;
    int i1 = rand() % 256;

    while (true) {
      ta.FlushFromCache();
      ta[i1] = 7;
      ssize_t i2 = ta.FindFirstCachedElementIndex();
      if (i1 == i2) break;
      ++j;
    }
  }

  std::cout << j << std::endl;
  return 0;
}

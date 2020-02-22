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
  int r = -1;
  while (r == -1) {
    ta.FlushFromCache();
    std::cout << ta[4] << std::endl;
    r = ta.FindFirstCachedElementIndex();
    std::cout << r << std::endl;
  }
  return 0;
}

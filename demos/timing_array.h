/*
 * Copyright 2020 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_TIMING_ARRAY_H_
#define DEMOS_TIMING_ARRAY_H_

#include <array>
#include <cstddef>
#include <vector>

// TODO: coalesce
#define TA_CACHE_LINE_SIZE 64
#define TA_PAGE_SIZE 4096
#define TA_CACHE_LINES_PER_PAGE (TA_PAGE_SIZE / TA_CACHE_LINE_SIZE)



// ARM
// http://infocenter.arm.com/help/topic/com.arm.doc.100095_0003_06_en/pat1406322717379.html
// Intel
// https://cpu.fyi/d/83c#G3.1121453
// AMD
// 

// TimingArray is an array optimized for inducing and measuring cache timing
// side-channels.
// elements designed not to interfere
class TimingArray {
 public:
  // ValueType is an alias for the element type of the array. Someday we might
  // want to make TimingArray a template class and take this as a parameter,
  // but for now the flexibility isn't worth the extra hassle.
  using ValueType = int;

  TimingArray();

  TimingArray(TimingArray&) = delete;
  TimingArray& operator=(TimingArray&) = delete;

  ValueType& operator[](size_t i) { return get_element(i); }
  size_t size() const { return pages_.size(); }

  void FlushFromCache();

  ssize_t FindFirstCachedElementIndex();
  ssize_t FindFirstCachedElementIndexAfter(size_t start);

 private:
  ValueType& get_element(size_t i);

  struct alignas(TA_CACHE_LINE_SIZE) CacheLine {
    ValueType value;
  };

  struct alignas(TA_PAGE_SIZE) Page {
    std::array<CacheLine, TA_CACHE_LINES_PER_PAGE> cache_lines;
  };

  // Use a `std::vector` here 
  std::vector<Page> pages_{256};
};

#endif  // DEMOS_TIMING_ARRAY_H_

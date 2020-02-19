/*
 * Copyright 2020 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_TIMING_H_
#define DEMOS_TIMING_H_

#include <array>
#include <cstddef>
#include <cstring>
#include <type_traits>

// TODO: coalesce
#define TA_CACHE_LINE_SIZE 64
#define TA_PAGE_SIZE 4096
#define TA_CACHE_LINES_PER_PAGE (TA_PAGE_SIZE / TA_CACHE_LINE_SIZE)

// TimingArray is an array optimized for inducing and measuring cache timing
// side-channels.
class TimingArray {
 public:
  // We could templatize this class, but for now we just use `int`.
  using ValueType = int;

  TimingArray() {
    // Ensure the entire array is backed by real pages
    memset(&pages_, 0xff, sizeof(pages_));
  }

  TimingArray(TimingArray&) = delete;
  TimingArray& operator=(TimingArray&) = delete;

  ValueType& operator[](size_t i) {
    // 113 works great for 256
    size_t page = (i * 113 + 100) % pages_.size();
    size_t cache_line = (i % TA_CACHE_LINES_PER_PAGE);

    return pages_[page].cache_lines[cache_line].value;
  }

  size_t size() const { return pages_.size(); }

  void FlushFromCache();
  ssize_t FindFirstCachedElementIndex();
  ssize_t FindFirstCachedElementIndexAfter(size_t start);

 private:
  struct alignas(TA_CACHE_LINE_SIZE) CacheLine {
    ValueType value;
  };

  struct alignas(TA_PAGE_SIZE) Page {
    std::array<CacheLine, TA_CACHE_LINES_PER_PAGE> cache_lines;
  };

  std::array<Page, 256> pages_;
  static_assert(sizeof(pages_) == 256 * TA_PAGE_SIZE);
};

#endif  // DEMOS_TIMING_H_

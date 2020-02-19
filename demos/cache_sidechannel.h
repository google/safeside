/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_CACHE_SIDECHANNEL_H_
#define DEMOS_CACHE_SIDECHANNEL_H_

#include <array>
#include <memory>

// Represents a cache-line in the oracle for each possible ASCII code.
// We can use this for a timing attack: if the CPU has loaded a given cache
// line, and the cache line it loaded was determined by secret data, we can
// figure out the secret byte by identifying which cache line was loaded.
// That can be done via a timing analysis.
//
// To do this, we create an array of 256 values, each of which do not share
// the same cache line as any other. To eliminate false positives due
// to prefetching, we also ensure no two values share the same page,
// by spacing them at intervals of 4096 bytes.
//
// See 2.3.5.4 Data Prefetching in the Intel Optimization Reference Manual:
//   "Data prefetching is triggered by load operations when [...]
//    The prefetched data is within the same 4K byte page as the load
//    instruction that triggered it."
//
// ARM also has this constraint on prefetching:
//   http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0388i/CBBIAAAA.html
//
// Spacing of 4096 was used in the original Spectre paper as well:
//   https://spectreattack.com/spectre.pdf
//
struct BigByte {
  // Explicitly initialize the array. It doesn't matter what we write; it's
  // only important that we write *something* to each page. Otherwise,
  // there's an opportunity for the range to be allocated as zero-fill-on-
  // demand (ZFOD), where all virtual pages are a read-only mapping to the
  // *same* physical page of zeros. The cache in modern Intel CPUs is
  // physically-tagged, so all of those virtual addresses would map to the
  // same cache line and we wouldn't be able to observe a timing difference
  // between accessed and unaccessed pages (modulo e.g. TLB lookups).
  std::array<unsigned char, 4096> padding_ = {};
};

// The first and last value might be adjacent to other elements on the heap,
// so we only add padding from both side and use only the other elements, which
// are guaranteed to be on different cache lines, and even different pages,
// than any other value.
struct PaddedOracleArray {
  BigByte pad_left;
  std::array<BigByte, 256> oracles_;
  BigByte pad_right;
};

// Provides an oracle of allocated memory indexed by 256 character ASCII
// codes in order to capture speculative cache loads.
//
// The client is expected to flush the oracle from the cache and access one of
// the indexing characters really (safe_offset_char) and one of them
// speculatively.
//
// Afterwards the client invokes the recomputation of the scores which
// computes which character was accessied speculatively and increases its
// score.
//
// This process (flushing oracle, speculative access of the oracle by the
// client and recomputation of scores) repeats until one of the characters
// accumulates a high enough score.
//
class CacheSideChannel {
 public:
  CacheSideChannel() = default;

  // Not copyable or movable.
  CacheSideChannel(const CacheSideChannel&) = delete;
  CacheSideChannel& operator=(const CacheSideChannel&) = delete;

  // Provides the oracle for speculative memory accesses.
  const std::array<BigByte, 256> &GetOracle() const;
  // Flushes all indexes in the oracle from the cache.
  void FlushOracle() const;
  // Finds which character was accessed speculatively and increases its score.
  // If one of the characters got a high enough score, returns true and that
  // character. Otherwise it returns false and any character that has the
  // highest score.
  std::pair<bool, char> RecomputeScores(char safe_offset_char);
  // Adds an artifical cache-hit and recompute scores. Useful for demonstration
  // that do not have natural architectural cache-hits.
  std::pair<bool, char> AddHitAndRecomputeScores();

 private:
  // Oracle array cannot be allocated for stack because MSVC stack size is 1MB,
  // so it would immediately overflow.
  std::unique_ptr<PaddedOracleArray> padded_oracle_array_ =
      std::unique_ptr<PaddedOracleArray>(new PaddedOracleArray);
  std::array<int, 257> scores_ = {};
};

#endif  // DEMOS_CACHE_SIDECHANNEL_H_


#ifndef EXPERIMENTAL_CACHE_SIZE_H_
#define EXPERIMENTAL_CACHE_SIZE_H_

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>

// TODO: depending on in which directory the file end up these might need to be
// changed
#include "../demos/asm/measurereadlatency.h"
#include "../demos/instr.h"
#include "../demos/utils.h"

// Determines the maximum latency of reading elements of a vector of size "sz",
// which is given as input.
// It reads each element of the vector and measures the time
// using MeasureReadLatency(). Later this number can be used to determine cache
// misses and hits
uint64_t FindMaxReadingTime(long sz) {
  // Generate a random order of accesses to eliminate the impact of hw
  // prefetchers
  std::vector<int64_t> accesses(sz);
  for (int64_t i = 0; i < sz; i++) {
    accesses.push_back(i);
  }
  std::random_device rd;
  std::mt19937 f(rd());
  std::shuffle(accesses.begin(), accesses.end(), f);

  std::vector<char> buf(sz);

  for (int64_t i : accesses) {
    ForceRead(&buf[i]);
  }

  //   Read each element in the same random order and keep track of the slowest
  //   read.
  uint64_t max_read_latency = std::numeric_limits<uint64_t>::min();
  for (int64_t i : accesses) {
    max_read_latency = std::max(max_read_latency, MeasureReadLatency(&buf[i]));
  }

  return max_read_latency;
}

// Determines the latency of reading the first element of a vector of size "sz",
// which is given as input, that was writtern to the cache before other elements
// It reads the first element based on the order of initial accesses and
// measures the time using MeasureReadLatency(). Later this number can be used
// to determine cache misses and hits
uint64_t FindFirstElementReadingTime(long sz) {
  // Generate a random order of accesses to eliminate the impact of hw
  // prefetchers
  std::vector<int64_t> accesses(sz);
  for (int64_t i = 0; i < sz; i++) {
    accesses.push_back(i);
  }
  std::random_device rd;
  std::mt19937 f(rd());
  std::shuffle(accesses.begin(), accesses.end(), f);

  std::vector<char> buf(sz);

  for (int64_t i : accesses) {
    ForceRead(&buf[i]);
  }

  return MeasureReadLatency(&buf[accesses[0]]);
}

#endif  // EXPERIMENTAL_CACHE_SIZE_H_

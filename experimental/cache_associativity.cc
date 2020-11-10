#include <stdlib.h>
#include <time.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>

// TODO: give compilation instructions instead of including nonlocal files
#include "../demos/asm/measurereadlatency.h"
#include "../demos/instr.h"
#include "../demos/utils.h"

// Determines the latency of reading the first element that was read in random
// order. Elements are "step" size apart from each other (starting from zero) in
// a vector of size "sz", which is given as input. It reads each element of the
// vector and measures the time using MeasureReadLatency(). Later this number
// can be used to determine cache misses and hits
uint64_t FindReadingTime(std::vector<char> buf, int64_t sz, int step) {
  // Generate a random order of accesses to eliminate the impact of hw
  // prefetchers
  std::vector<int64_t> accesses(sz);
  for (int64_t i = 0; i < sz; i += step) {
    accesses.push_back(i);
  }
  std::random_device rd;
  std::mt19937 f(rd());
  std::shuffle(accesses.begin(), accesses.end(), f);

  for (int64_t i : accesses) {
    ForceRead(&buf[i]);
  }

  // Read each element in the same random order and keep track of the slowest
  // read.
  // uint64_t max_read_latency = 0;
  //   for (int64_t i : accesses) {
  //     max_read_latency += std::max(max_read_latency,
  //     MeasureReadLatency(&buf[i]));
  //   }

  return MeasureReadLatency(&buf[accesses[0]]);
}

// analyzes a ranges of numbers to find the size of strides that fill a cache
// set and the next read of that stride causes a cache eviction

int cache_set_analysis() {
  // size of the cache under analysis
  static constexpr int64_t kCacheSize = 256 * 1024;

  static constexpr int64_t kStrideMaxSize = 128 * 1024;
  static constexpr int64_t kStrideMinSize = 8;
  static constexpr int iterations = 1000;

  std::cout << "writing timing results..." << std::endl;

  FILE* f = fopen("cache_set_l2.csv", "w");
  if (!f) return 1;

  for (int i = 0; i < iterations; i++) {
    for (int64_t step = kStrideMinSize; step <= kStrideMaxSize; step *= 2) {
      // size of the buffer under analysis
      int64_t sz = kCacheSize + step;
      std::vector<char> buf(sz);

      fprintf(f, "%d, %lu\n", step, FindReadingTime(buf, sz, step));
      std::cout << ".";
      std::flush(std::cout);
    }
  }

  fclose(f);
  return 0;
}

int main() {
  cache_set_analysis();

  std::cout << "done" << std::endl;
  return 0;
}

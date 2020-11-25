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

// Determines the average latency of reading a subset of elements of a vector
// "buf" from memory to cache
// It reads elements that are located "step" size from each other (i.e. 0, step,
// 2*step, 3* step, ...) in a random order and measures the time using
// MeasureReadLatency(). Later this number is used to determine cache line sizes
double_t FindReadingTime(std::vector<char> buf, int64_t sz, int step) {
  int64_t accesses_size = sz / step;
  std::vector<int64_t> accesses(accesses_size);
  for (int64_t i = 0; i < sz; i += step) {
    accesses.push_back(i);
  }
  std::random_device rd;
  std::mt19937 f(rd());
  std::shuffle(accesses.begin(), accesses.end(), f);

  uint64_t total_read_latency = 0;

  for (int64_t i : accesses) {
    total_read_latency += MeasureReadLatency(&buf[i]);
  }

  return total_read_latency / accesses_size;
}

// Analyzes a range of numbers to find the size of strides that is bigger than
// the cache line size, so not all cache lines are needed to be read from memory
// to cache
// Note that (1) the time of reading from cache is negiligble compared to
// reading from memory, and (2) CPUs read cache lines from memory instead byte
// by byte.
// Therefore, the time for reading a fixed size buffer (smaller than cache size)
// in chunks of size n should be around the same value as long as n < cache line
// size (the average reading time increases as n increase (i.e. total time/
// number of reads)), and then as n increases less number of cache lines are
// read from memory and takes less time.

int cache_line_size_analysis() {
  static constexpr int64_t kMaxSize = 256;
  static constexpr int64_t kMinSize = 4;
  static constexpr int iterations = 100;

  int64_t sz = 4 * 1024 * 1024;
  int64_t cache_size = 8 * 1024 * 1024;
  std::vector<char> buf(sz);
  std::vector<char> cache_flusher(cache_size);

  std::cout << "writing timing results..." << std::endl;

  FILE* f = fopen("cache_line_size_results.csv", "w");
  if (!f) return 1;

  for (int j = 0; j < iterations; j++) {
    // analyzes a range of memory sizes to find the maximum time needed to read
    // each of their elements
    for (int64_t step = kMinSize; step <= kMaxSize; step++) {
      
      // clean the cache by loading another it with another vector
      // makes sure all elements of "buf" are evicted from cache
      for (int64_t i = 0; i < cache_size; i++) {
        ForceRead(&cache_flusher[i]);
        cache_flusher[i]++;
      }

      // computes the average time for reading every "step" element in "buf"
      double_t res = FindReadingTime(buf, sz, step);
      fprintf(f, "%d, %f\n", step, res);
      std::cout << ".";
      std::flush(std::cout);
    }
  }

  fclose(f);
  return 0;
}

int main() {
  cache_line_size_analysis();

  std::cout << "done" << std::endl;
  return 0;
}

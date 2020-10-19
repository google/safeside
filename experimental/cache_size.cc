#include <stdlib.h>
#include <time.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>

//TODO: give compilation instructions instead of including nonlocal files
#include "../demos/asm/measurereadlatency.h"
#include "../demos/instr.h"
#include "../demos/utils.h"

// Determines the maximum latency of reading elements of a vector of size "sz",
// which is given as input.
// It reads each element of the vector and measures the time
// using MeasureReadLatency(). Later this number can be used to determine cache
// misses and hits
uint64_t AnalyzeReadingTime(int64_t sz) {
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

  // TODO: not sure about the size of chuncks and its impact on prefetchers
  for (int64_t i : accesses) {
    ForceRead(&buf[i]);
  }

  // Read each element in the same random order and keep track of the slowest
  // read.
  uint64_t max_read_latency = std::numeric_limits<uint64_t>::min();
  for (int64_t i : accesses) {
    max_read_latency = std::max(max_read_latency, MeasureReadLatency(&buf[i]));
  }

  return max_read_latency;
}

int main() {

  std::cout << "writing timing results to \"results.csv\"" << std::endl;

  FILE* f = fopen("results.csv", "w");
  if (!f) return 1;

  static constexpr int64_t kMaxSize = 16 * 1024 * 1024;
  static constexpr int64_t kMinSize = 1024;
  static constexpr int iterations = 100;

  for (int i = 0; i < iterations; i++) {
    // analyzes a range of memory sizes to find the maximum time needed to read
    // each of their elements
    for (int64_t sz = kMinSize; sz <= kMaxSize; sz = sz * 1.5) {
      fprintf(f, "%d, %lu\n", sz, AnalyzeReadingTime(sz));
      std::cout << ".";
      std::flush(std::cout);
    }
  }

  fclose(f);

  std::cout << "done" << std::endl;
  return 0;
}

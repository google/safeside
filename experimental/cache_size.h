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

// Returns a shuffled vector of integers from 0 (inclusive) to n (exclusive).
// This can be used to randomize access order.
inline std::vector<size_t> ShuffledRange(size_t n) {
  std::vector<size_t> numbers;
  numbers.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    numbers.push_back(i);
  }
  std::random_device rd;
  std::mt19937 f(rd());
  std::shuffle(numbers.begin(), numbers.end(), f);
  return numbers;
}

// Determines the maximum latency of reading elements of a vector of size "sz",
// which is given as input.
// It reads each element of the vector and measures the time
// using MeasureReadLatency(). Later this number can be used to determine cache
// misses and hits
inline uint64_t FindMaxReadingTime(size_t buffer_size) {
  std::vector<size_t> accesses = ShuffledRange(buffer_size);

  std::vector<char> buffer;
  buffer.reserve(buffer_size);

  for (size_t i : accesses) {
    ForceRead(&buffer[i]);
  }

  // Read each element in the same random order and keep track of the slowest
  // read.
  uint64_t max_read_latency = 0;
  for (size_t i : accesses) {
    max_read_latency = std::max(max_read_latency, MeasureReadLatency(&buffer[i]));
  }

  return max_read_latency;
}

// Determines the latency of reading the first element of a vector of size "sz",
// which is given as input, that was writtern to the cache before other elements
// It reads the first element based on the order of initial accesses and
// measures the time using MeasureReadLatency(). Later this number can be used
// to determine cache misses and hits
inline uint64_t FindFirstElementReadingTime(size_t buffer_size) {
  std::vector<size_t> accesses = ShuffledRange(buffer_size);

  std::vector<char> buffer;
  buffer.reserve(buffer_size);

  for (size_t i : accesses) {
    ForceRead(&buffer[i]);
  }

  return MeasureReadLatency(&buffer[accesses[0]]);
}

#endif  // EXPERIMENTAL_CACHE_SIZE_H_

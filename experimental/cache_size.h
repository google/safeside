#ifndef EXPERIMENTAL_CACHE_SIZE_H_
#define EXPERIMENTAL_CACHE_SIZE_H_

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <assert.h>

// TODO: depending on in which directory the file end up these might need to be
// changed
#include "../demos/asm/measurereadlatency.h"
#include "../demos/instr.h"
#include "../demos/utils.h"


// A permutation function for 1 byte.
inline unsigned char PermuteChar(unsigned char x) {
  // LCG
  return x * 113 + 100;
}

// A permutation function for an int, with a specified maximum.
// Rather than going all at once and building an LCG for `0..max`,
// we compose this out of 1-4 calls to PermuteChar, permuting 8 bits at a time.
inline uint32_t PermuteInt(uint32_t x, uint32_t max) {
  int num_bytes;
  if (max <= 1<<8) {
      num_bytes = 1;
  } else if (max <= 1 << 16) {
      num_bytes = 2;
  } else if (max <= 1 << 24) {
      num_bytes = 3;
  } else {
      num_bytes = 4;
  }

  unsigned char* begin_inclusive = reinterpret_cast<unsigned char*>(&x);
  unsigned char* end_inclusive = reinterpret_cast<unsigned char*>(&x + (num_bytes - 1));
  for (unsigned char* c = begin_inclusive; c <= end_inclusive; ++c) {
    // Keep running permutations on c until we get an in-range value.
    // This will only execute more than once for the last byte, and only if max
    // isn't exactly 1 << (n*8).
    do {
        *c = PermuteChar(*c);
    } while (x >= max);
  }
  return x;
}

template <typename T> class ShuffledSpan {
 public:
  ShuffledSpan(const std::vector<T>& arr): begin_(arr.size() != 0 ? &arr[0] : nullptr), size_(arr.size()) {}

  ShuffledSpan(const ShuffledSpan&) = default;
  ShuffledSpan& operator=(const ShuffledSpan&) = default;
  ShuffledSpan(ShuffledSpan&&) = default;
  ShuffledSpan& operator=(ShuffledSpan&&) = default;

  size_t size() const {
      return size_;
  }

  const T& operator[](size_t i) const {
    i = PermuteInt(i, size_);
    return begin_[i];
  }

 private:
  const T* begin_;
  int32_t size_;
};

// Determines the maximum latency of reading elements of a vector of size "sz",
// which is given as input.
// It reads each element of the vector and measures the time
// using MeasureReadLatency(). Later this number can be used to determine cache
// misses and hits
inline uint64_t FindMaxReadingTime(size_t buffer_size) {
  std::vector<char> buffer(buffer_size);
  ShuffledSpan<char> shuffled(buffer);

  for (size_t i = 0; i < shuffled.size(); ++i) {
    ForceRead(&shuffled[i]);
  }

  // Read each element in the same random order and keep track of the slowest
  // read.
  uint64_t max_read_latency = 0;
  for (size_t i = 0; i < shuffled.size(); ++i) {
    max_read_latency = std::max(max_read_latency, MeasureReadLatency(&shuffled[i]));
  }

  return max_read_latency;
}

// Determines the latency of reading the first element of a vector of size "sz",
// which is given as input, that was writtern to the cache before other elements
// It reads the first element based on the order of initial accesses and
// measures the time using MeasureReadLatency(). Later this number can be used
// to determine cache misses and hits
inline uint64_t FindFirstElementReadingTime(size_t buffer_size) {
  std::vector<char> buffer(buffer_size);
  ShuffledSpan<char> shuffled(buffer);

  for (size_t i = 0; i < shuffled.size(); ++i) {
    ForceRead(&shuffled[i]);
  }

  return MeasureReadLatency(&shuffled[0]);
}

#endif  // EXPERIMENTAL_CACHE_SIZE_H_

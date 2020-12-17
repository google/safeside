
#include "cache_size.h"

#include <iostream>
#include <vector>
// Measures how often measuring time is able to accurately determine cache hits
// and cache misses based on the buffer size under analysis and the cache size
// it checks whether buffer sizes that fit in cache have less reading time or
// not
int test_timing() {
  static constexpr long kCacheSize = 8 * 1024 * 1024;
  static constexpr long kIterations = 2;
  static constexpr long kTestSizes = 4;

  // generates a list of numbers that are used to determine the size of buffers
  std::vector<int> candidates;
  for (int i = 0; i < kTestSizes; ++i) {
    candidates.push_back(20 * (i + 1));
  }
  int total_passed_tests = 0;
  for (int i = 0; i < kIterations; i++) {
    int passed_tests = 0;
    for (int candidate_large : candidates) {
      uint64_t res_large = FindMaxReadingTime(kCacheSize * candidate_large);

      for (int candidate_small : candidates) {
        uint64_t res_small = FindMaxReadingTime(kCacheSize / candidate_small);
        // we expect that buffer sizes < cache size always have less reading
        // times as they all should result in cache hits while buffer sizes >
        // cache size suffer from cache misses, so they must take longer to be
        // read
        if (res_large <= res_small) {
          std::cout << "test failed" << std::endl;
        } else {
          std::cout << "test passed" << std::endl;
          passed_tests++;
        }
      }
    }
    std::cout << "In total out of " << kTestSizes * kTestSizes
              << " comparisons " << passed_tests << " were successfully passed."
              << std::endl;
    total_passed_tests += passed_tests;
  }
  float res =
      total_passed_tests * 100 / (kIterations * kTestSizes * kTestSizes);
  std::cout << res << "% of tests passed." << std::endl;
  if (res > 99) return 0;
  return 1;
}


// Smoke-tests PermuteInt.
int test_permuting() {
  int rv;
  int permuted = PermuteInt(0, /*max=*/1);
  if ( permuted != 0) {
    std::cout << "PermuteInt(0, /*max=*/1) = " << permuted << ", expected 0\n";
    rv = 1;
  }
  for (int i = 0; i < 257; ++i) {
    int permuted = PermuteInt(i, /*max=*/257);
    if (permuted >= 257) {
      std::cout << "PermuteInt(" << i << ", /*max=*/257) = " << permuted << ", expected < 257\n";
      rv = 1;
    }
  }
  return rv;
}


// TODO: absltest... :(

int main() {
  int rv = 0;
  if (test_timing()) {
    std::cout << "test_timing failed";
    rv = 1;
  }
  if (test_permuting()) {
    std::cout << "test_permuting failed";
  }
  return rv;
}

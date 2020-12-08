
#include "cache_size.h"

#include <iostream>

// Measures how often measuring time is able to accurately determine cache hits
// and cache misses based on the buffer size under analysis and the cache size.
// Checks whether buffer sizes that fit in cache are faster.
int main() {
  static constexpr int64_t kCacheSize = 8 * 1024 * 1024;
  static constexpr int kIterations = 2;
  static constexpr int kTestSizes = 4;

  // generates a list of numbers that are used to determine the size of buffers
  std::vector<int> candidates;
  for (int i = 0; i < kTestSizes; ++i) {
    candidates.push_back(20 * (i + 1));
  }
  int total_passed_tests = 0;
  for (int i = 0; i < kIterations; ++i) {
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
          ++passed_tests;
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

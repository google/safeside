
#include "cache_size.h"

#include <iostream>
// Measure how often measuring time is able to accurately determine cache hits
// and cache misses based on the buffer size under analysis and the cache size
// it checks whether buffer sizes that fit in cache have less reading time or
// not
int main(int argc, char* argv[]) {
  static constexpr long kCacheSize = 8 * 1024 * 1024;
  static constexpr long kIterations = 2;
  static constexpr long kTestSizes = 4;

  // generates a list of numbers that are used to determine the size of buffers
  std::vector<int> candidates(kTestSizes);
  for (int i = 1; i < kTestSizes + 1; i++) {
    candidates.at(i - 1) = 2 * i;
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
  std::cout << total_passed_tests * 100 /
                   (kIterations * kTestSizes * kTestSizes)
            << "% of tests passed." << std::endl;

  return 0;
}

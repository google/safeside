
#include "cache_size.h"

#include <iostream>

int main(int argc, char* argv[]) {
  static constexpr long kCacheSize = 8 * 1024 * 1024;
  static constexpr long kIterations = 2;
  static constexpr long kTestSizes = 4;

  std::vector<int> candidates(kTestSizes);
  for (int i = 0; i < kTestSizes; i++) {
    candidates.at(i) = i + 1;
  }

  for (int i = 0; i < kIterations; i++) {
    int passed_tests = 0;
    for (int candidate_large : candidates) {
      uint64_t res_large =
          FindFirstElementReadingTime(kCacheSize * candidate_large);

      for (int candidate_small : candidates) {
        uint64_t res_small =
            FindFirstElementReadingTime(kCacheSize / candidate_small);

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
  }

  return 0;
}

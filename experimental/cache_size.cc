#include "cache_size.h"

#include <stdlib.h>
#include <time.h>

// Analyzes a ranges of sizes to find the cache size based on timing
// information.
// The user should manually analyze the results and pinpoints the cache size where there
// is jump in average latency time (cache misses happened).
int CacheSizeAnalysis() {
  static constexpr int64_t kMaxSize = 32 * 1024 * 1024;
  static constexpr int64_t kMinSize = 1024;
  static constexpr int kIterations = 20;

  std::cout << "writing timing results..." << std::endl;

  FILE* f = fopen("cache_size_results.csv", "w");
  if (!f) return 1;

  for (int i = 0; i < kIterations; ++i) {
    // analyzes a range of memory sizes to find the maximum time needed to read
    // each of their elements
    for (int64_t size = kMinSize; size <= kMaxSize; size *= 1.5) {
      fprintf(f, "%d, %lu\n", size  , FindMaxReadingTime(size));
      std::cout << ".";
      std::flush(std::cout);
    }
  }

  fclose(f);
  return 0;
}

int main() {
  int res = CacheSizeAnalysis();
  if (res == 0) {
    std::cout << "Cache size analysis succeeded" << std::endl;
  } else {
    std::cout << "Cache size analysis failed" << std::endl;
  }
  return 0;
}

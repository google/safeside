#include "cache_size.h"

#include <stdlib.h>
#include <time.h>

// analyzes a ranges of sizes to find the cache size based on timing
// information
// manually a user analyzes the results and pinpoints the cache size where there
// is jump in average latency time (cache misses happened)
int cache_size_analysis() {
  static constexpr int64_t kMaxSize = 32 * 1024 * 1024;
  static constexpr int64_t kMinSize = 1024;
  static constexpr int kIterations = 20;

  std::cout << "writing timing results..." << std::endl;

  FILE* f = fopen("cache_size_results.csv", "w");
  if (!f) return 1;

  for (int i = 0; i < kIterations; i++) {
    // analyzes a range of memory sizes to find the maximum time needed to read
    // each of their elements
    for (int64_t sz = kMinSize; sz <= kMaxSize; sz = sz * 1.5) {
      fprintf(f, "%d, %lu\n", sz, FindFirstElementReadingTime(sz));
      std::cout << ".";
      std::flush(std::cout);
    }
  }

  fclose(f);
  return 0;
}

int main() {
  int res = cache_size_analysis();
  if (res == 0) {
    std::cout << "Cache size analysis was successfully done" << std::endl;
  } else {
    std::cout << "Cache size analysis failed" << std::endl;
  }
  return 0;
}

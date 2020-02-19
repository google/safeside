#include <iostream>

#include "instr.h"
#include "timing.h"
#include "utils.h"

int main(int argc, char* argv[]) {
  TimingArray ta;
  int j = 0;
  for (int n = 0; n < 10000; ++n) {
    // std::cout << n << std::endl;
    int i1 = rand() % 256;

    while (true) {
      ta.FlushFromCache();
      ta[i1] = 7;
      ssize_t i2 = ta.FindFirstCachedElementIndex();
      if (i1 == i2) break;
      ++j;
    }
  }

  std::cout << j << std::endl;
  return 0;
}

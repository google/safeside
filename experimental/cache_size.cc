#include <stdlib.h>
#include <time.h>

#include <algorithm>
#include <iostream>

int64_t MAX_SIZE = 16 * 1024 * 1024;
int64_t MIN_SIZE = 1024;
int64_t ACCESSES_NUM = 64 * 1024 * 1024;
int REPEATS = 50;

clock_t whack_cache(const int64_t sz, int64_t* accesses) {
  char* buf = new char[sz];

  clock_t start = clock();
  // TODO fencing
  for (int64_t i = 0; i < ACCESSES_NUM; i++) {
    ++buf[(accesses[i] * 64) % sz];
  }
  // TODO: elapsed time generates zero
  clock_t elapsed = clock() - start;

  double cpu_time_used = ((double)elapsed) / CLOCKS_PER_SEC;

  delete[] buf;
  return elapsed;
}

void shuffle(int64_t* a) {
  int64_t i = ACCESSES_NUM - 1;
  srand(time(NULL));
  while (i >= 0) {
    int64_t rand_num = rand() % (i + 1);
    int64_t temp = a[i];
    a[i] = a[rand_num];
    a[rand_num] = temp;
    i--;
  }
}

int main() {
  std::cout << "writing timing results to \"results.csv\"" << std::endl;

  FILE* f = fopen("results.csv", "w");
  if (!f) return 1;

  int64_t* accesses = new int64_t[ACCESSES_NUM];
  for (int64_t i = 0; i < ACCESSES_NUM; i++) {
    accesses[i] = i;
  }
  shuffle(accesses);

  for (int i = 0; i < REPEATS; i++) {
    for (int64_t sz = MIN_SIZE; sz <= MAX_SIZE; sz = sz * 1.2) {
      fprintf(f, "%d, %lu\n", sz, whack_cache(sz, accesses));
      std::cout << ".";
      fflush(stdout);
    }
  }

  fclose(f);

  std::cout << "done" << std::endl;
  return 0;
}

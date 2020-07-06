/**
Copyright 2020 Google LLC

Licensed under both the 3-Clause BSD License and the GPLv2, found in the
LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.

SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
**/

#include <chrono>
#include <iostream>
// This is a microbenchmark that can be used to compare the performance costs of
// different Spectre variant 1 mitigations.
//
// This benchmark attempts to access an array 999 times with an in bounds index
// and then once with an out of bounds index. This is repeated ten times. The
// goal is to train the branch predictor to always take true branch, then to
// cause speculative execution with the 1000th access with an out of bounds
// index. The benchmark measures the total time taken for all of the iterations.
int main() {
  constexpr int kArrLen = 999;
  int arr[kArrLen];
  for (int i = 0; i < kArrLen; i++) {
    arr[i] = 1;
  }

  int sum = 0;
  auto start = std::chrono::system_clock::now();
  for (int i = 0; i < 10000; i++) {
    int j = i % 1000;
    if (j < kArrLen) {
      sum += arr[j];
    }
  }
  auto end = std::chrono::system_clock::now();
  auto elapsed = end - start;
  std::cout << elapsed.count() << std::endl;
}

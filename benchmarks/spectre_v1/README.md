# Benchmarks of Spectre v1 Mitigations

This folder has microbenchmarks to see the differences between the performance
of Spectre v1 mitigations at a small scale.

Microbenchmarks often do not reflect the performance costs of a mitigation in
context, but they are useful for discussion when the limitations of
microbenchmarks are considered along with the results.

## Build instructions

```
$ bash
$ cd safeside
$ cmake -B build .
$ make -C build

# Everything should be built now.

# Run the microbenchmark with no mitigation enabled.
$ ./build/benchmarks/spectre_v1/unmitigated
84733

# If your compiler is Clang (>8.0.0), then this is also built.
# Run the microbenchmark with speculative load hardening enabled.
./build/benchmarks/spectre_v1/slh_mitigated
193599

# If you want to explicitly set your compiler to clang.

$ CC=clang CXX=clang++ cmake -B build .

# On systems like Ubuntu 18.04 where the system installed Clang's version is too
# out of date to build slh_mitigated.
$ sudo apt install clang-10
$ CC=clang-10 CXX=clang++-10 cmake -B build .

```

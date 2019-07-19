# Sidechannel project

Sidechannel project aims to test synthetically the effectiveness of mitigations
against sidechannel information leaks. To make that possible, we are going to
build a test suite that robustly demonstrates information leaks across a
breadth of techniques (Meltdown, Spectre variants, L1TF, MDS, and those yet to
be discovered) and across a variety of isolation boundaries (same process,
user/kernel, VM guest/host, network).

## Tested environments

We currently test our changes on:
Linux - Intel Xeon Gold 6154 - {g++-6.4.0 - g++-8.0.1, clang-4.0 - clang-7}
Linux - {Intel Core i7-6700, AMD Ryzen 5 PRO 2400G} - {g++-5.4.0 - g++-9.1.0}
Linux - Intel Core2 Quad - g++-8.1.1
Linux - Intel XeonE5-2670 - g++-4.8.4
Linux - Intel Core i7-3520M - {g++-8.3.0, clang-6.0 - clang-7, icc-19.0.4.243}
Windows 10 on Google Cloud - Intel Haswell - {MSVC2019 x86 release build,
MSVC2019 x64 release build}
MacOS - Intel Core i7-8750H - clang Apple LLVM 10.0.1
Linux - ARMv8 Cavium ThunderX2 T99 - g++-7.3.0
Linux - PowerPC POWER9 Boston 2.2 - g++-8.3.0

## Collaboration

See the [contributing instructions](./CONTRIBUTING.md).

## Disclaimer

This is not an officially supported Google product.

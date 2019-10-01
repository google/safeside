# Demo programs

## Build instructions

```bash
cd safeside
mkdir build
cd build
cmake ..
make

# Everything should be built now.

./spectre_v1

# You need to load the kernel module before running this
sudo ./meltdown

./spectre_v4

./ret2spec
```

## Tested environments

We currently test our changes on:
- Linux - Intel Xeon Gold 6154 - {g++-6.4.0 - g++-8.0.1, clang-4.0 - clang-7}
- Linux - {Intel Core i7-6700, AMD Ryzen 5 PRO 2400G} - {g++-5.4.0 - g+- +-9.1.0,
- clang-6.0 - clang-8}
- Linux - Intel Core2 Quad - g++-8.1.1
- Linux - Intel XeonE5-2670 - g++-4.8.4
- Linux - Intel Core i7-3520M - {g++-8.3.0, clang-6.0 - clang-7, - icc-19.0.4.243}
- Windows 10 on Google Cloud - Intel Haswell - {MSVC2019 x86 release build,
- MSVC2019 x64 release build}
- MacOS - Intel Core i7-8750H - clang Apple LLVM 10.0.1
- Linux - ARMv8 Cavium ThunderX2 T99 - g++-7.3.0
- Linux - PowerPC POWER9 Boston 2.2 - g++-8.3.0

# Demo programs

## Build instructions

```bash
cd safeside
cmake -B build .
make -C build

# Everything should be built now.

./build/demos/spectre_v1_pht_sa

./build/demos/spectre_v1_btb_ca

# You need to load the kernel module before running this
sudo ./build/demos/meltdown

./build/demos/spectre_v4

./build/demos/ret2spec_sa

etc.
```

## Naming Scheme

The naming scheme is heavily influenced by [A Systematic Evaluation of Transient Execution Attacks and Defenses](https://arxiv.org/pdf/1811.05441.pdf). So for example, `spectre_v1_btb_ca.cc` is a demonstration of using a mistrained speculative branch (Spectre v1) via mistraining the branch target buffer (BTB) to transmit data cross-address-space (CA). (As for what counts as Spectre v1, see the discussion in [PR #12](https://github.com/google/safeside/pull/12).)

## Tested environments

We currently test our changes on:
- Linux - Intel Xeon Gold 6154 - {g++-6.4.0 - g++-8.0.1, clang-4.0 - clang-7}
- Linux - {Intel Core i7-6700, AMD Ryzen 5 PRO 2400G} - {g++-5.4.0 - g+- +-9.1.0,
  clang-6.0 - clang-8}
- Linux - Intel Core2 Quad - g++-8.1.1
- Linux - Intel XeonE5-2670 - g++-4.8.4
- Linux - Intel Core i7-3520M - {g++-8.3.0, clang-6.0 - clang-7, - icc-19.0.4.243}
- Windows 10 on Google Cloud - Intel Haswell - {MSVC2019 x86 release build,
  MSVC2019 x64 release build}
- MacOS - Intel Core i7-8750H - clang Apple LLVM 10.0.1
- Linux - ARMv8 Cavium ThunderX2 T99 - g++-7.3.0
- Linux - PowerPC POWER9 Boston 2.2 - g++-8.3.0

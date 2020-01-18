# Assembly primitives

This folder holds functions that, for various reasons, we could only implement in assembly.

## Why?

When should something be implemented here rather than in C/C++, maybe with inline assembly?

-  Need precise control over the instructions executed, e.g. choosing exactly when a parameter is read from the stack into a register.
-  Need implementation to stay instruction-for-instruction the same, e.g. comparing cycle counts for reads  across identical machines using different compilers.
-  MSVC x64 doesn't support inline assembly and we've seen MSVC x86 reorder instructions in violation of semantics, e.g. reordering `LFENCE` and `RDTSC` relative to each other (https://godbolt.org/z/mSve-d).

## Functions

### `MeasureReadLatency`

This is the core primitive for cache timing side-channel attacks. And it should be pretty easy! Conceptually it's just four steps:
1.  Read timestamp _T\_before_ from some platform-specific timestamp counter
2.  Read from memory
3.  Read timestamp _T\_after_
4.  Return _T\_after_ - _T\_before_

The hard part is ensuring that the memory read actually happens **entirely between** (1) and (3), _and_ that it's the **only** thing that happens between those two points. Otherwise we're measuring more (or less) than just the memory read.

Some ways things could go wrong due to out-of-order execution:
-  The memory read happens before we read _T\_before_
-  The memory read isn't finished before we read _T\_after_
-  The read of _T\_before_ migrates up ahead of prior instructions
-  Some previously-issued memory operation is in-flight and completes between the reads of _T\_before_ and _T\_after_, e.g. a cache flush or unrelated write.

We use a lot of serializing instructions and memory fences to avoid these. See the per-platform implementations for more details.

It's also possible that we get unlucky and `MeasureReadLatency` gets preempted by the OS while performing the timed read, which might mean we return a very high latency value for a read that hit L1 cache. There's not a lot we can do about that; we leave the job of repeating the measurement and dealing with outliers as an exercise for the caller.

## Platform and toolchain quirks

### Decorators (underscore prefix)

Some assembly files define the symbol `Function`. Others define `_Function`. This is an artifact of different platform ABIs: some ABIs add a leading underscore to C symbols so they exist in a separate namespace from (undecorated) pure-assembler symbols.

Specifically:
-  Windows (Portable Executable or PE format) decorates `__cdecl` C functions with a leading underscore, except on 64-bit platforms. ([ref](https://docs.microsoft.com/en-us/cpp/build/reference/decorated-names?view=vs-2019#FormatC))
-  macOS (Mach-O format) decorates C functions with a leading underscore. ([ref](https://web.archive.org/web/20040720060835/http://developer.apple.com/documentation/DeveloperTools/Conceptual/MachORuntime/MachORuntime.pdf), see pg. 18, "Searching for Symbols")
-  Linux (ELF) does _not_ decorate C function names: "External C symbols have the same names in C and object files' symbol tables" ([ref](https://refspecs.linuxfoundation.org/elf/gabi41.pdf), see pg. 4-22)

### Assemblers and syntax

We need to implement the same function across two toolchains (GCC/Clang and MSVC) and four architectures (`x86`, `x86_64`, `aarch64`, and `ppc64le`). Thankfully, the matrix is sparse: we only need to support MSVC for `x86` and `x86_64`. (At least [for now](https://docs.microsoft.com/en-us/cpp/build/configuring-programs-for-arm-processors-visual-cpp?view=vs-2019).)

Microsoft Macro Assembler (MASM), the assembler in the MSVC toolchain, is not an easy tool to work with. Unlike the GCC/Clang assemblers, it:
-  Can only use Intel syntax on `x86`/`x86_64`
-  Can't preprocess code before assembling it
-  Introduces comments with `COMMENT` or `;` instead of `/* */` or `//`

To make it easier to share as much of the implementations as possible, we start off writing the GCC/Clang implementations in `.S` (assembly-with-preprocessing) files with C-style comments. We use the `.intel_syntax noprefix` directive so that we're writing instructions in a format MASM will be able to read. Finally, we run that code through `gcc -EP` to remove comments and formatting, and put that in the `.asm` file for MASM.

If we add more assembly implementations, we may end up automating this process more or moving to NASM.

## Useful references

-  [x86 32-bit calling conventions in MSVC](https://devblogs.microsoft.com/oldnewthing/20040108-00/?p=41163)
-  [AMD64 calling convention in MSVC](https://devblogs.microsoft.com/oldnewthing/20040114-00/?p=41053)
-  [PowerPC ISA+ABI (first post in a series)](https://devblogs.microsoft.com/oldnewthing/20180806-00/?p=99425)
-  [Calling conventions](https://en.wikipedia.org/wiki/Calling_convention) and [x86 calling conventions](https://en.wikipedia.org/wiki/X86_calling_conventions) on Wikipedia

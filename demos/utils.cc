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

#include <cstddef>

#include "instr.h"
#include "utils.h"

constexpr size_t kCacheLineSize = 64;

// Forces a memory read of the byte at address p. This will result in the byte
// being loaded into cache.
void ForceRead(const void *p) {
  (void)*reinterpret_cast<const volatile char *>(p);
}

// Flush a memory interval from cache.
void FlushFromCache(const char *start, const char *end) {
  // Start on the first byte and continue in kCacheLineSize steps.
  for (const char *ptr = start; ptr < end; ptr += kCacheLineSize) {
    CLFlush(ptr);
  }
  // Flush explicitly the last byte.
  CLFlush(end - 1);
}

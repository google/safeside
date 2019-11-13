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

#ifndef DEMOS_UTILS_H
#define DEMOS_UTILS_H

#include "compiler_specifics.h"

// Forced memory load. Loads the memory into cache. Used during both real and
// speculative execution to create a microarchitectural side effect in the
// cache. Also used for latency measurement in the FLUSH+RELOAD technique.
// Should be inlined to minimize the speculation window.
SAFESIDE_ALWAYS_INLINE
inline void ForceRead(const void *p) {
  (void)*reinterpret_cast<const volatile char *>(p);
}

// Flush a memory interval from cache. Used to induce speculative execution on
// flushed values until they are fetched back to the cache.
void FlushFromCache(const char *start, const char *end);
#endif  // DEMOS_UTILS_H

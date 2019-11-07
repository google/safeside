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

// Forced memory load. Used during both real and speculative execution to create
// a microarchitectural side effect in the cache. Also used for latency
// measurement in the FLUSH+RELOAD technique.
void ForceRead(const void *p) {
  (void)*reinterpret_cast<const volatile char *>(p);
}

// Flush a memory interval from cache. Used to induce speculative execution on
// flushed values until they are fetched back to the cache.
void FlushFromCache(const char *start, const char *end) {
  // Start on the first byte and continue in kCacheLineSize steps.
  for (const char *ptr = start; ptr < end; ptr += kCacheLineSize) {
    CLFlush(ptr);
  }
  // Flush explicitly the last byte.
  CLFlush(end - 1);
}

static void SignalHandler(
    int /* signum */, siginfo_t * /* siginfo */, void *context) {
  // On IA32, X64 and PPC moves the instruction pointer to the
  // "afterspeculation" label. On ARM64 moves the instruction pointer to the
  // "LocalHandler" label.
  ucontext_t *ucontext = static_cast<ucontext_t *>(context);
#if SAFESIDE_LINUX
#  if SAFESIDE_IA32
  ucontext->uc_mcontext.gregs[REG_EIP] =
      reinterpret_cast<greg_t>(afterspeculation);
#  elif SAFESIDE_X64
  ucontext->uc_mcontext.gregs[REG_RIP] =
      reinterpret_cast<greg_t>(afterspeculation);
#  elif SAFESIDE_ARM64
  ucontext->uc_mcontext.pc = reinterpret_cast<greg_t>(LocalHandler);
#  elif SAFESIDE_PPC
  ucontext->uc_mcontext.regs->nip =
      reinterpret_cast<uintptr_t>(afterspeculation);
#  else
#    error Unsupported CPU.
#  endif
#elif SAFESIDE_MAC
#  if SAFESIDE_IA32
  ucontext->uc_mcontext->__ss.__eip =
      reinterpret_cast<uintptr_t>(afterspeculation);
#  elif SAFESIDE_X64
  ucontext->uc_mcontext->__ss.__rip =
      reinterpret_cast<uintptr_t>(afterspeculation);
#else
#  error Unsupported OS.
#endif
}

// Sets up signal handling that moves the instruction pointer to the
// afterspeculation (or LocalHandler in case of ARM) label.
void OnSignalMoveRipToAfterspeculation(int signal) {
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_sigaction = SignalHandler;
  act.sa_flags = SA_SIGINFO;
  sigaction(signal, &act, nullptr);
}

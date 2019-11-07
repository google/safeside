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
#ifndef DEMOS_LOCAL_LABELS_H
#define DEMOS_LOCAL_LABELS_H

#include "compiler_specifics.h"

// Generic strings used across examples. The public_data is intended to be
// accessed in the C++ execution model. The content of the private_data is
// intended to be leaked outside of the C++ execution model using sidechannels.
// Concrete sidechannel is dependent on the concrete vulnerability that we are
// demonstrating.
const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";

#if SAFESIDE_ARM64
// Local handler necessary for avoiding local/global linking mismatches on ARM.
// When we use extern char[] declaration for a label defined in assembly, the
// compiler yields this sequence that fails loading the actual address of the
// label:
// adrp x0, :got:label
// ldr x0, [x0, #:got_lo12:label]
// On the other hand when we use this local handler, the compiler yield this
// sequence of instructions:
// adrp x0, label
// add x0, x0, :lo12:label
// and that works correctly because it if an effective equivalent of
// adr x0, label.
static void LocalHandler() {
  asm volatile("b afterspeculation");
}
#endif

#if SAFESIDE_LINUX || SAFESIDE_MAC
void SignalHandler(
    int /* signum */, siginfo_t * /* siginfo */, void *context) {
  // On IA32, X64 and PPC moves the instruction pointer to the
  // "afterspeculation" label. On ARM64 moves the instruction pointer to the
  // "LocalHandler" label.
  ucontext_t *ucontext = static_cast<ucontext_t *>(context);
#if SAFESIDE_LINUX && SAFESIDE_IA32
  ucontext->uc_mcontext.gregs[REG_EIP] =
      reinterpret_cast<greg_t>(afterspeculation);
#elif SAFESIDE_LINUX && SAFESIDE_X64
  ucontext->uc_mcontext.gregs[REG_RIP] =
      reinterpret_cast<greg_t>(afterspeculation);
#elif SAFESIDE_LINUX && SAFESIDE_ARM64
  ucontext->uc_mcontext.pc = reinterpret_cast<greg_t>(LocalHandler);
#elif SAFESIDE_LINUX && SAFESIDE_PPC
  ucontext->uc_mcontext.regs->nip =
      reinterpret_cast<uintptr_t>(afterspeculation);
#elif SAFESIDE_MAC && SAFESIDE_IA32
  ucontext->uc_mcontext->__ss.__eip =
      reinterpret_cast<uintptr_t>(afterspeculation);
#elif SAFESIDE_MAC && SAFESIDE_X64
  ucontext->uc_mcontext->__ss.__rip =
      reinterpret_cast<uintptr_t>(afterspeculation);
#else
#  error Unsupported OS/CPU combination.
#endif
}
#endif

#endif  // DEMOS_LOCAL_LABELS_H

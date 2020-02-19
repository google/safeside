/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_MELTDOWN_LOCAL_CONTENT_H_
#define DEMOS_MELTDOWN_LOCAL_CONTENT_H_

#include "compiler_specifics.h"

#if SAFESIDE_LINUX || SAFESIDE_MAC
#include <signal.h>
#endif

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

static void SignalHandler(
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

// Sets up signal handling that moves the instruction pointer to the
// afterspeculation (or LocalHandler in case of ARM) label. See the commentary
// to LocalHandler function why we must treat ARM differently.
static void OnSignalMoveRipToAfterspeculation(int signal) {
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_sigaction = SignalHandler;
  act.sa_flags = SA_SIGINFO;
  sigaction(signal, &act, nullptr);
}

#endif  // DEMOS_MELTDOWN_LOCAL_CONTENT_H_

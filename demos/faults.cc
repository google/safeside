/*
 * Copyright 2020 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include "faults.h"

#include <setjmp.h>

#include <cstring>

namespace {

// The fault handler that should be called from the signal handler, and the
// sigjmp context that should be jumped to after that.
//
// Declaring these thread-local might be a bit aspirational -- they definitely
// *shouldn't* be shared across threads, but the current implementation doesn't
// necessarily guarantee it's safe to use RunWithFaultHandler on two threads
// without external synchronization.
thread_local PosixFaultHandler fault_handler;
thread_local sigjmp_buf signal_handler_jmpbuf;

void SignalHandler(int signal, siginfo_t *info, void *ucontext) {
  fault_handler(signal, info, ucontext);
  siglongjmp(signal_handler_jmpbuf, 1);
}

}  // namespace

bool RunWithFaultHandler(std::function<void()> inner,
                         PosixFaultHandler handler) {
  struct sigaction sa = {};
  sa.sa_sigaction = SignalHandler;

  struct sigaction oldsa;
  // This sets the signal handler for the entire process.
  sigaction(SIGSEGV, &sa, &oldsa);

  fault_handler = handler;
  bool handled_fault = true;

  // Use sigsetjmp/siglongjmp to save and restore signal mask. Otherwise we
  // will jump out of the signal handler and leave the currently-being-handled
  // signal blocked. The result of a SIGSEGV being raised while blocked is
  // "undefined"[1], but in practice leads to killing the process.
  //
  // [1] https://www.man7.org/linux/man-pages/man2/sigprocmask.2.html#NOTES
  if (sigsetjmp(signal_handler_jmpbuf, 1) == 0) {
    inner();
    handled_fault = false;
  }

  sigaction(SIGSEGV, &oldsa, nullptr);
  fault_handler = {};

  return handled_fault;
}

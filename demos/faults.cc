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

// The sigjmp context we should jump to after catching the fault.
sigjmp_buf signal_handler_jmpbuf;

extern "C"
void SignalHandler(int signal, siginfo_t *info, void *ucontext) {
  siglongjmp(signal_handler_jmpbuf, 1);
}

}  // namespace

bool RunWithFaultHandler(int fault_signum, std::function<void()> inner) {
  struct sigaction sa = {};
  sa.sa_sigaction = SignalHandler;

  struct sigaction oldsa;
  // This sets the signal handler for the entire process.
  sigaction(fault_signum, &sa, &oldsa);

  // Use sigsetjmp/siglongjmp to save and restore signal mask. Otherwise we will
  // jump out of the signal handler and leave the currently-being-handled signal
  // blocked. The result of a SIGBUS, SIGFPE, SIGILL, or SIGSEGV being raised
  // while blocked is "undefined"[1], but in practice the process is killed.
  //
  // [1] https://www.man7.org/linux/man-pages/man2/sigprocmask.2.html#NOTES
  bool handled_fault = true;
  if (sigsetjmp(signal_handler_jmpbuf, 1) == 0) {
    inner();
    handled_fault = false;
  }

  sigaction(fault_signum, &oldsa, nullptr);

  return handled_fault;
}

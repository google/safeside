/*
 * Copyright 2020 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_FAULTS_H_
#define DEMOS_FAULTS_H_

#include <signal.h>

#include <functional>

// See "The siginfo_t argument to a SA_SIGINFO handler"
// at https://www.man7.org/linux/man-pages/man2/sigaction.2.html
using PosixFaultHandler = std::function<void(int, siginfo_t*, void*)>;

// Run `inner` with a signal handler installed to catch failure signals,
// currently defined as `SIGSEGV`. If such a signal is raised, the signal
// handler calls `handler` and then returns from RunWithFaultHandler.
//
// Returns true iff a signal was handled.
bool RunWithFaultHandler(std::function<void()> inner,
                         PosixFaultHandler handler);

// Convenience adapter for callers that don't need signal details.
inline bool RunWithFaultHandler(std::function<void()> inner,
                                std::function<void()> handler) {
  return RunWithFaultHandler(inner,
                             [handler](int, siginfo_t*, void*) { handler(); });
}

#endif  // DEMOS_FAULTS_H_

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

// Run `inner` with a signal handler installed for `fault_signum`, which is
// probably a signal that maps to a synchronous hardware exception: SIGSEGV,
// SIGILL, SIGFPE, SIGTRAP, or SIGBUS.
//
// If that signal is raised, the execution of `inner` is aborted.
//
// Not thread-safe. Don't use from more than one thread at a time.
//
// Returns true iff a signal was handled.
bool RunWithFaultHandler(int fault_signum, std::function<void()> inner);

#endif  // DEMOS_FAULTS_H_

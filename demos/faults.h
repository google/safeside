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

// Run `inner` with a signal handler installed to catch failure signals,
// currently defined as `SIGSEGV`. If such a signal is raised, the execution
// of `inner` is aborted.
//
// Not thread-safe. Don't use from more than one thread at a time.
//
// Returns true iff a signal was handled.
bool RunWithFaultHandler(std::function<void()> inner);

#endif  // DEMOS_FAULTS_H_

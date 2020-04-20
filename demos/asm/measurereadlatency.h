/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_ASM_MEASUREREADLATENCY_H_
#define DEMOS_ASM_MEASUREREADLATENCY_H_

#include <cstdint>

// Reads a byte from *address and returns a measure of how long it took by
// sampling a platform timer before and after the read.
//
// The measurement may include the time it took to execute some other
// instructions, but implementations go to some trouble to ensure all
// *variability* across measurements is due to the latency of the memory read.
//
// See README.md and platform-specific implementations for more discussion.
//
// Will return spuriously high results if e.g. the thread is preempted while
// measuring the read.
extern "C" uint64_t MeasureReadLatency(const void* address);

#endif  // DEMOS_ASM_MEASUREREADLATENCY_H_

/*
 * Copyright 2020 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_HARDWARE_CONSTANTS_H_
#define DEMOS_HARDWARE_CONSTANTS_H_

#include <cstddef>

#include "compiler_specifics.h"

// Constants describing properties of the current processor architecture.
#if SAFESIDE_PPC
constexpr size_t kCacheLineBytes = 128;
constexpr size_t kPageBytes = 64 * 1024;
#else
constexpr size_t kCacheLineBytes = 64;
constexpr size_t kPageBytes = 4 * 1024;
#endif

#endif  // DEMOS_HARDWARE_CONSTANTS_H_

/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_COMPILER_SPECIFICS_H_
#define DEMOS_COMPILER_SPECIFICS_H_

#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
#define SAFESIDE_X64 1
#elif defined(__i386__) || defined(_M_IX86)
#define SAFESIDE_IA32 1
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
#define SAFESIDE_ARM64 1
#elif defined(__powerpc64__) || defined(__PPC64__) || defined(__powerpc__) || \
    defined(__ppc__) || defined(__PPC__)
#define SAFESIDE_PPC 1
#else
#  error Undefined architecture.
#endif

#ifdef _MSC_VER
#define SAFESIDE_MSVC 1
#define SAFESIDE_NEVER_INLINE __declspec(noinline)

// If a function is modified by both `inline` and `SAFESIDE_ALWAYS_INLINE`,
// MSVC may issue a diagnostic:
//     warning C4141: 'inline': used more than once
//
// To avoid this, put `inline` *before* `SAFESIDE_ALWAYS_INLINE` on any
// function that will be compiled by MSVC.
#define SAFESIDE_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__)
#define SAFESIDE_GNUC 1
#define SAFESIDE_NEVER_INLINE __attribute__((noinline))
#define SAFESIDE_ALWAYS_INLINE __attribute__((always_inline))
#else
#  error Unknown compiler.
#endif

#ifdef __linux__
#define SAFESIDE_LINUX 1
#elif defined(__APPLE__)
#define SAFESIDE_MAC 1
#endif

#endif  // DEMOS_COMPILER_SPECIFICS_H_

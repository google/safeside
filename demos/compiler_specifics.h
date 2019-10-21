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

#ifndef SAFESIDE_COMPILER_SPECIFICS
#define SAFESIDE_COMPILER_SPECIFICS

#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
#define SAFESIDE_X64
#elif defined(__i386__) || defined(_M_IX86)
#define SAFESIDE_IA32
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
#define SAFESIDE_ARM64
#elif defined(__powerpc64__) || defined(__PPC64__) || defined(__powerpc__) || \
    defined(__ppc__) || defined(__PPC__)
#define SAFESIDE_PPC
#else
#  error Undefined architecture.
#endif

#ifdef _MSC_VER
#define SAFESIDE_NOINLINE __declspec(noinline)
#define SAFESIDE_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__)
#define SAFESIDE_NOINLINE __attribute__((noinline))
#define SAFESIDE_ALWAYS_INLINE __attribute__((always_inline))
#else
#  error Unknown compiler.
#endif

#endif

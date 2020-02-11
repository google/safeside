/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

/**
 * Speculation over ERET, HVC and SMC instruction.
 *
 * In this example we demonstrate a behavior of ARM CPUs that speculate across
 * instructions whose equivalents are blocking on other architectures. Those are
 * the ERET instruction that returns from kernel to userspace, from hypervisor
 * to kernel etc., the HVC instruction that enters hypervisor from kernel and
 * the SMC instruction that enters the secure monitor from kernel or from
 * hypervisor.
 *
 * Since these instructions behave as an undefined instruction in a userspace
 * program we have to execute them in kernel code using a Linux kernel module.
 * For the sake of portability and maintainability we execute them there only
 * speculatively and verify that the speculative execution does not block on any
 * of those instructions by invoking all three of them before we yield a memory
 * access into oracle that loads a userspace-provided address into the cache.
 *
 * We use our userspace infrastructure for the setup of the oracle and for the
 * FLUSH+RELOAD technique. The kernel receives hexadecimal addresses that are
 * written into a SYSFS file /sys/kernel/safeside_eret_hvc_smc/address
 * During each sysfs store the kernel code performs a Spectre v1 gadget in
 * order to achieve speculative execution. The speculatively executed
 * architecturally unreachable code begins with ERET, HVC and SMC instructions
 * followed by a memory access instruction. Afterwards the control flow returns
 * back to userspace where we verify that the provided index in memory oracle
 * was speculatively accessed. */

#include "compiler_specifics.h"

#if !SAFESIDE_LINUX
#  error Unsupported OS. Linux required.
#endif

#if !SAFESIDE_ARM64
#  error Unsupported architecture. ARM64 required.
#endif

#include <array>
#include <cstring>
#include <fstream>
#include <iostream>

#include "cache_sidechannel.h"
#include "instr.h"
#include "local_content.h"
#include "utils.h"

// Userspace wrapper of the eret_hvc_smc kernel module.
// Writes userspace addresses into a SYSFS file while the kernel handler
// accesses those adresses speculatively after it speculates over ERET, HVC and
// SMC instructions.
static char LeakByte(const char *data, size_t offset) {
  CacheSideChannel sidechannel;

  for (int run = 0;; ++run) {
    std::ofstream out("/proc/safeside_eret_hvc_smc/address");
    if (out.fail()) {
      std::cerr << "Eret_hvc_smc module not loaded or not running as root."
                << std::endl;
      exit(EXIT_FAILURE);
    }

    sidechannel.FlushOracle();

    // Sends the secret address in the oracle to the kernel so that it's
    // accessed only in there and only speculatively.
    out << std::hex << static_cast<const void *>(
        sidechannel.GetOracle().data() + static_cast<size_t>(data[offset]));
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
    std::cout << LeakByte(private_data, i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

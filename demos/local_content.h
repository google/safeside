/*
 * Copyright 2019 Google LLC
 *
 * Licensed under both the 3-Clause BSD License and the GPLv2, found in the
 * LICENSE and LICENSE.GPL-2.0 files, respectively, in the root directory.
 *
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
 */

#ifndef DEMOS_LOCAL_CONTENT_H_
#define DEMOS_LOCAL_CONTENT_H_

// Generic strings used across examples. The public_data is intended to be
// accessed in the C++ execution model. The content of the private_data is
// intended to be leaked outside of the C++ execution model using sidechannels.
// Concrete sidechannel is dependent on the concrete vulnerability that we are
// demonstrating.
const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";

#endif  // DEMOS_LOCAL_CONTENT_H_

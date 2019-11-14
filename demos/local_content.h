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

#ifndef DEMOS_LOCAL_CONTENT_H
#define DEMOS_LOCAL_CONTENT_H
// Generic strings used across examples. The public_data is intended to be
// accessed in the C++ execution model. The content of the private_data is
// intended to be leaked outside of the C++ execution model using sidechannels.
// Concrete sidechannel is dependent on the concrete vulnerability that we are
// demonstrating.
const char *public_data = "Hello, world!";
const char *private_data = "It's a s3kr3t!!!";
#endif  // DEMOS_LOCAL_CONTENT_H

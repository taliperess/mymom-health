// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include "pw_function/function.h"

namespace sense {

/// Interface for a worker that can ambiently execute functions.
///
/// Current implementations delegate to work queues.
class Worker {
 public:
  /// Ambiently execute a function.
  virtual void RunOnce(pw::Function<void()>&& work) = 0;

 protected:
  ~Worker() = default;
};

}  // namespace sense

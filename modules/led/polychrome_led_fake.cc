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

#include "modules/led/polychrome_led_fake.h"

namespace sense {

bool PolychromeLedFake::is_on() const {
  return red() != 0 || green() != 0 || blue() != 0;
}

void PolychromeLedFake::EnableWaiting() {
  red_.set_sync(true);
  green_.set_sync(true);
  blue_.set_sync(true);
}

void PolychromeLedFake::Await() {
  red_.Await();
  green_.Await();
  blue_.Await();
}

bool PolychromeLedFake::TryAwait() {
  return red_.TryAwait() && green_.TryAwait() && blue_.TryAwait();
}

}  // namespace sense

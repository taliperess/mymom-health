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

#include "modules/led/monochrome_led_fake.h"
#include "modules/led/polychrome_led_fake.h"
#include "system/system.h"

namespace sense::system {

sense::MonochromeLed& MonochromeLed() {
  static MonochromeLedFake monochrome_led;
  return monochrome_led;
}

sense::PolychromeLed& PolychromeLed() {
  static PolychromeLedFake polychrome_led;
  return polychrome_led;
}

}  // namespace sense::system

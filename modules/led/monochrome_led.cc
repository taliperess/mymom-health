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

#include "modules/led/monochrome_led.h"
#include "pw_assert/check.h"

namespace am {

/// Reference to the ``MonochromeLed`` instance returned by the system.
///
/// Since the PWM callbacks must be free functions that take no argument, this
/// reference is used to access the LED. This imposes the restriction that unit
/// tests that may create multiple ``MonochromeLed`` instances MUST NOT run
/// concurrently.
static MonochromeLed* singleton = nullptr;

static void DoPulse() {
  static uint16_t counter = 0;
  uint16_t brightness = 0;
  if (counter < 0x100) {
    brightness = counter;
  } else {
    brightness = 0x200 - counter;
  }
  singleton->SetBrightness(brightness * brightness);
  counter = (counter + 1) % 0x200;
}

void MonochromeLed::Pulse(uint32_t interval_ms) {
  PW_CHECK_PTR_EQ(singleton, nullptr);
  singleton = this;
  SetCallback(DoPulse, 0x200, interval_ms);
  SetState(State::kPwm);
  singleton = nullptr;
}

}  // namespace am

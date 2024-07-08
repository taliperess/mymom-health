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

// SimpleLED implementation for the rp2040 using the pico-sdk.

#include "modules/led/monochrome_led.h"
#include "pico/stdlib.h"
#include "pw_digital_io_rp2040/digital_io.h"

namespace am {

static pw::digital_io::Rp2040DigitalInOut led({
    .pin = PICO_DEFAULT_LED_PIN,
    .polarity = pw::digital_io::Polarity::kActiveHigh,
});

MonochromeLed::MonochromeLed() {
  led.Enable();
  Set(false);
}

void MonochromeLed::Set(bool enable) {
  led.SetState(enable ? pw::digital_io::State::kActive
                      : pw::digital_io::State::kInactive);
}

}  // namespace am

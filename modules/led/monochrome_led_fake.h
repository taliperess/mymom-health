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

#include "modules/led/monochrome_led.h"
#include "modules/pwm/digital_out_fake.h"
#include "pw_chrono/system_clock.h"
#include "pw_digital_io/digital_io_mock.h"

namespace sense {

class MonochromeLedFake : public MonochromeLed {
 public:
  static constexpr size_t kCapacity = 256;

  using Clock = pw::digital_io::DigitalInOutMockImpl::Clock;
  using Event = pw::digital_io::DigitalInOutMockImpl::Event;
  using State = pw::digital_io::DigitalInOutMockImpl::State;

  MonochromeLedFake() : MonochromeLedFake(Clock::RealClock()) {}

  explicit MonochromeLedFake(Clock& clock)
      : MonochromeLed(led_sio_, led_pwm_), led_sio_(clock) {
    TurnOff();
  }

  pw::InlineDeque<Event>& events() { return led_sio_.events(); }

 private:
  pw::digital_io::DigitalInOutMock<kCapacity> led_sio_;
  PwmDigitalOutFake led_pwm_;
};

}  // namespace sense

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

namespace sense {

MonochromeLed::MonochromeLed(pw::digital_io::DigitalInOut& sio,
                             PwmDigitalOut& pwm)
    : sio_(sio), pwm_(pwm) {}

bool MonochromeLed::IsOn() {
  if (GetMode() != Mode::kSio) {
    return false;
  }
  auto result = sio_.GetState();
  if (!result.ok()) {
    return false;
  }
  return *result == State::kActive;
}

void MonochromeLed::TurnOn() {
  SetMode(Mode::kSio);
  PW_CHECK_OK(sio_.SetState(State::kActive));
}

void MonochromeLed::TurnOff() {
  SetMode(Mode::kSio);
  PW_CHECK_OK(sio_.SetState(State::kInactive));
}

void MonochromeLed::SetBrightness(uint16_t level) {
  SetMode(Mode::kPwm);
  pwm_.SetLevel(level);
}

void MonochromeLed::Toggle() {
  auto result = sio_.GetState();
  bool is_off =
      mode_ != Mode::kSio || !result.ok() || *result == State::kInactive;
  SetMode(Mode::kSio);
  PW_CHECK_OK(sio_.SetState(is_off ? State::kActive : State::kInactive));
}

void MonochromeLed::Pulse(uint32_t interval_ms) {
  SetMode(Mode::kPwm);
  pwm_.SetCallback(
      [this]() {
        static uint16_t counter = 0;
        uint16_t brightness = 0;
        if (counter < 0x100) {
          brightness = counter;
        } else {
          brightness = 0x200 - counter;
        }
        pwm_.SetLevel(brightness * brightness);
        counter = (counter + 1) % 0x200;
      },
      0x200,
      interval_ms);
}

void MonochromeLed::SetMode(Mode mode) {
  if (mode == mode_) {
    return;
  }
  switch (mode_) {
    case Mode::kSio:
      PW_CHECK_OK(sio_.Disable());
      pwm_.Enable();
      break;
    case Mode::kPwm:
      pwm_.Disable();
      PW_CHECK_OK(sio_.Enable());
      break;
  }
  mode_ = mode;
}

}  // namespace sense

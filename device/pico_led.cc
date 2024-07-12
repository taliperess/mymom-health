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

#include "device/pico_led.h"

#include "modules/led/monochrome_led.h"
#include "pico/stdlib.h"

namespace am {

static constexpr uint16_t kPin = PICO_DEFAULT_LED_PIN;
static constexpr pw::digital_io::Polarity kPolarity =
    pw::digital_io::Polarity::kActiveHigh;

PicoLed::PicoLed()
    : led_({kPin, kPolarity}), pwm_gpio_(kPin, kPolarity) {
  led_.Enable();
  led_.SetState(pw::digital_io::State::kInactive);
}

MonochromeLed::State PicoLed::GetState() {
  if (gpio_get_function(kPin) == GPIO_FUNC_PWM) {
    return State::kPwm;
  } else {
    return *(led_.IsStateActive()) ? State::kOn : State::kOff;
  }
}

void PicoLed::SetState(State state) {
  if (state_ == state) {
    return;
  }
  switch (state_) {
    case State::kOff:
    case State::kOn:
      led_.Disable();
      break;
    case State::kPwm:
      pwm_gpio_.Disable();
      break;
  }
  switch (state) {
    case State::kOff:
      led_.Enable();
      led_.SetState(pw::digital_io::State::kInactive);
      break;
    case State::kOn:
      led_.Enable();
      led_.SetState(pw::digital_io::State::kActive);
      break;
    case State::kPwm:
      pwm_gpio_.Enable();
      break;
  }
  state_ = state;
}

void PicoLed::DoSetBrightness(uint16_t level) {
  SetState(State::kPwm);
  pwm_gpio_.SetLevel(level);
}

void PicoLed::SetCallback(Callback callback, uint16_t per_interval, uint32_t interval_ms) {
  pwm_gpio_.SetCallback(callback, per_interval, interval_ms);
}

}  // namespace am

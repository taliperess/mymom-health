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

#include "device/pico_pwm_gpio.h"
#include "modules/led/monochrome_led.h"
#include "modules/led/polychrome_led.h"
#include "pico/stdlib.h"
#include "pw_digital_io_rp2040/digital_io.h"
#include "system/system.h"

namespace am::system {

static constexpr pw::digital_io::Rp2040Config kDefaultLedConfig = {
    .pin = PICO_DEFAULT_LED_PIN,
    .polarity = pw::digital_io::Polarity::kActiveHigh,
};

am::MonochromeLed& MonochromeLed() {
  static ::pw::digital_io::Rp2040DigitalInOut led_sio(kDefaultLedConfig);
  static ::am::PicoPwmGpio led_pwm(kDefaultLedConfig);
  static ::am::MonochromeLed led(led_sio, led_pwm);
  return led;
}

static constexpr pw::digital_io::Rp2040Config kRedLedConfig = {
    .pin = 6,
    .polarity = pw::digital_io::Polarity::kActiveLow,
};

static constexpr pw::digital_io::Rp2040Config kGreenLedConfig = {
    .pin = 7,
    .polarity = pw::digital_io::Polarity::kActiveLow,
};

static constexpr pw::digital_io::Rp2040Config kBlueLedConfig = {
    .pin = 10,
    .polarity = pw::digital_io::Polarity::kActiveLow,
};

am::PolychromeLed& PolychromeLed() {
  static ::am::PicoPwmGpio red_pwm(kRedLedConfig);
  static ::am::PicoPwmGpio green_pwm(kGreenLedConfig);
  static ::am::PicoPwmGpio blue_pwm(kBlueLedConfig);
  static ::am::PolychromeLed rgb_led(red_pwm, green_pwm, blue_pwm);
  return rgb_led;
}

}  // namespace am::system

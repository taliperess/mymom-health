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

#include <cstdint>

#include "modules/pwm/digital_out.h"
#include "pw_digital_io/digital_io.h"

namespace sense {

/// Interface for a simple LED.
class MonochromeLed {
 public:
  using Callback = void (*)();
  using State = ::pw::digital_io::State;

  MonochromeLed(pw::digital_io::DigitalInOut& sio, PwmDigitalOut& pwm);
  ~MonochromeLed() = default;

  /// Returns whether the LED is on.
  bool IsOn();

  /// Turns on the LED.
  void TurnOn();

  /// Turns off the LED.
  void TurnOff();

  /// Sets the brightness of the LED.
  ///
  /// This method will automatically swith the LED to PWM mode.
  ///
  /// @param  level   Relative brightness of the LED
  void SetBrightness(uint16_t level);

  /// Turns the LED on if it is off, or off otherwise.
  void Toggle();

  /// Fades the LED on and off continuously.
  ///
  /// This method will automatically swith the LED to PWM mode.
  ///
  /// @param  interval_ms   The duration of a fade cycle, in milliseconds.
  void Pulse(uint32_t interval_ms);

 private:
  /// Indicates whether the LED is on, off, or pulse-width-modulated.
  enum class Mode {
    kSio,
    kPwm,
  };

  Mode GetMode() const { return mode_; }
  void SetMode(Mode mode);

  Mode mode_ = Mode::kPwm;
  pw::digital_io::DigitalInOut& sio_;
  PwmDigitalOut& pwm_;
};

}  // namespace sense

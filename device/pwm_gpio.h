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

#include <chrono>
#include <cstdint>

#include "hardware/pwm.h"
#include "modules/led/monochrome_led.h"
#include "pw_chrono/system_clock.h"
#include "pw_digital_io/polarity.h"

namespace am {

/// This class represents a GPIO being driven by the PWM block.
class PwmGpio {
 public:
  using Callback = MonochromeLed::Callback;

  PwmGpio(uint16_t pin, pw::digital_io::Polarity polarity);

  ~PwmGpio();

  uint16_t slice_num() const { return slice_num_; }

  /// Sets the callback to invoke periodically.
  ///
  /// The behavior of the callback may vary cyclically, i.e. fading an LED on
  /// and off. This method sets the IRQ wrap value and clock divider so that
  /// the callback is invoked the correct number of times in each interval of
  /// time.
  ///
  /// If ``Disable`` is called, this method must be called again before
  /// ``Enable`` to restore its behavior.
  ///
  /// @param  callback      Function to invoke repeatedly.
  /// @param  per_interval  Number of time the function should be invoked per
  ///                       elaped interval.
  /// @param  interval_ms   The dureation of each interval, in milliseconds.
  void SetCallback(Callback callback, uint16_t per_interval, uint32_t interval_ms);

  /// Sets the GPIO to be driven by the PWM block.
  ///
  /// If a callback is set, also enables the IRQ for the PWM wrap value.
  void Enable();

  /// Resets the GPIO to a default configuration.
  ///
  /// This will clear the current callback.
  void Disable();

  /// Sets the output level of the GPIO.
  void SetLevel(uint16_t level);

 private:
  uint16_t pin_;
  pw::digital_io::Polarity polarity_;
  uint16_t slice_num_;
  pwm_config config_;
  Callback callback_ = nullptr;
};

}  // namespace am

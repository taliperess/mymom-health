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

namespace am {

/// Interface for a simple LED.
class MonochromeLed {
 public:
  using Callback = void (*)();

  virtual ~MonochromeLed() = default;

  /// Returns whether the LED is on.
  bool IsOn() {
    return GetState() == State::kOn;
    ;
  };

  /// Turns on the LED.
  void TurnOn() { SetState(State::kOn); }

  /// Turns off the LED.
  void TurnOff() { SetState(State::kOff); }

  /// Sets the brightness of the LED.
  ///
  /// This method will automatically swith the LED to PWM mode.
  ///
  /// @param  level   Relative brightness of the LED
  void SetBrightness(uint16_t level) { DoSetBrightness(level); }

  /// Turns the LED on if it is off, or off otherwise.
  void Toggle() {
    SetState(GetState() == State::kOff ? State::kOn : State::kOff);
  }

  /// Fades the LED on and off continuously.
  ///
  /// This method will automatically swith the LED to PWM mode.
  ///
  /// @param  interval_ms   The duration of a fade cycle, in milliseconds.
  void Pulse(uint32_t interval_ms);

 protected:
  MonochromeLed();

  /// Indicates whether the LED is on, off, or pulse-width-modulated.
  enum class State {
    kOn,
    kOff,
    kPwm,
  };

  virtual State GetState() = 0;
  virtual void SetState(State state) = 0;

  /// @copydoc ``MonochromeLed::SetBrightness``
  virtual void DoSetBrightness(uint16_t level) = 0;

  /// Sets a callback to invoke periodically.
  ///
  /// The behavior of the callback may vary cyclically, i.e. fading the LED on
  /// and off. The callback will invoked the correct number of times in each
  /// interval of time.
  ///
  /// This method will automatically swith the LED to PWM mode.
  ///
  /// @param  callback      Function to invoke repeatedly.
  /// @param  per_interval  Number of time the function should be invoked per
  ///                       elaped interval.
  /// @param  interval_ms   The dureation of each interval, in milliseconds.
  virtual void SetCallback(Callback callback,
                           uint16_t per_interval,
                           uint32_t interval_ms) = 0;
};

}  // namespace am

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

namespace am {

/// Interface for a simple LED.
class MonochromeLed {
 public:
  MonochromeLed();
  virtual ~MonochromeLed() = default;

  /// Returns whether the LED is on.
  bool IsOn() const { return led_is_on_; };

  /// Turns on the LED.
  void TurnOn();

  /// Turns off the LED.
  void TurnOff();

  // Turns the LED on if it is off, or off if it is on.
  void Toggle();

 protected:
  /// Turns the LED on the board on or off.
  ///
  /// @param  enable  True turns the LED on; false turns it off.
  virtual void Set(bool enable);

 private:
  bool led_is_on_ = false;
};

}  // namespace am

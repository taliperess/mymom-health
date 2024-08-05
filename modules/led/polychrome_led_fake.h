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

#include "modules/led/polychrome_led.h"
#include "modules/pwm/digital_out_fake.h"
#include "pw_chrono/system_clock.h"

namespace sense {

/// Interface for a simple LED.
class PolychromeLedFake : public PolychromeLed {
 public:
  PolychromeLedFake() : PolychromeLed(red_, green_, blue_) {}

  uint16_t red() const { return red_.level(); }
  uint16_t green() const { return green_.level(); }
  uint16_t blue() const { return blue_.level(); }

  /// Returns whether any LED component has a non-zero level.
  bool is_on() const;

  /// Enables "synchronous mode".
  ///
  /// When enabled, each call to `SetColor` or `SetBrightness` will block until
  /// another thread calls `Await`, `TryAwait`, or `TryAwaitUntil`.
  void EnableWaiting();

  /// Blocks until a call to `SetColor` or `SetBrightness` has been made.
  void Await();

  /// Returns whether a call to `SetColor` or `SetBrightness` has been made.
  bool TryAwait();

  /// Returns whether `SetColor` or `SetBrightness` is called before the given
  /// expiration.
  template <typename Duration>
  bool TryAwaitFor(Duration duration);

 private:
  PwmDigitalOutFake red_;
  PwmDigitalOutFake green_;
  PwmDigitalOutFake blue_;
};

// Template method implementations.

template <typename Duration>
bool PolychromeLedFake::TryAwaitFor(Duration duration) {
  auto expiration = pw::chrono::SystemClock::TimePointAfterAtLeast(duration);
  return red_.TryAwaitUntil(expiration) && green_.TryAwaitUntil(expiration) &&
         blue_.TryAwaitUntil(expiration);
}

}  // namespace sense

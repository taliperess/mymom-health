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
#include <cstddef>
#include <cstdint>

#include "modules/indicators/system_led.h"
#include "pw_chrono/system_clock.h"
#include "pw_containers/vector.h"

namespace am {

// This class is a fake implementation of ``SystemLed`` that allows capturing
// sequences of on/off toggles.
class SystemLedForTest : public SystemLed {
 public:
  static constexpr size_t kCapacity = 256;

  pw::chrono::SystemClock::duration interval() const { return interval_; }

  void set_interval_ms(uint32_t interval_ms) {
    interval_ = std::chrono::milliseconds(interval_ms);
  }

  /// Returns on/off intervals encoded as follows: the top bit indicates whether
  /// the LED was on or off, and the lower 7 bits indicate for how many
  /// intervals, up to a max of 127.
  const pw::Vector<uint8_t>& GetOutput() { return output_; }

  /// Encodes the parameters in the same manner as ``GetOutput``.
  static uint8_t Encode(bool is_on, size_t num_intervals);

 private:
  /// @copydoc ``SystemLed::SetLed``.
  void Set(bool enable) override;

  pw::chrono::SystemClock::duration interval_ = std::chrono::milliseconds(1);
  pw::chrono::SystemClock::time_point last_;
  pw::Vector<uint8_t, kCapacity> output_;
};

}  // namespace am

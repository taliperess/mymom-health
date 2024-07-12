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

#include "modules/led/monochrome_led.h"
#include "pw_chrono/system_clock.h"
#include "pw_containers/vector.h"

namespace am {

// This class is a fake implementation of ``MonochromeLed`` that allows
// capturing sequences of on/off toggles.
class MonochromeLedFake : public MonochromeLed {
 public:
  static constexpr size_t kCapacity = 256;

  MonochromeLedFake() { set_interval_ms(1); }

  pw::chrono::SystemClock::duration interval() const { return interval_; }

  uint32_t interval_ms() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(interval_)
        .count();
  }

  void set_interval_ms(uint32_t interval_ms) {
    interval_ = pw::chrono::SystemClock::for_at_least(
        std::chrono::milliseconds(interval_ms));
  }

  /// Encodes the parameters as follows: the top bit indicates whether
  /// the LED was on or off, and the lower 7 bits indicate for how many
  /// intervals, up to a max of 127.
  static uint8_t Encode(bool is_on, size_t num_intervals);

  /// Returns on/off intervals encoded by ``Encode``.
  const pw::Vector<uint8_t>& GetOutput() { return output_; }

  /// Clears the saved output.
  void ResetOutput() { output_.clear(); }

 protected:
  /// @copydoc ``MonochromeLed::Set``.
  void Set(bool enable) override;

 private:
  pw::chrono::SystemClock::duration interval_;
  pw::chrono::SystemClock::time_point last_;
  pw::Vector<uint8_t, kCapacity> output_;
};

}  // namespace am

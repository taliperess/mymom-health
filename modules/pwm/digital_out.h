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

#include "pw_chrono/system_clock.h"
#include "pw_function/function.h"

namespace sense {

/// This class represents an output being driven by the PWM block.
class PwmDigitalOut {
 public:
  using Callback = ::pw::Function<void()>;

  virtual ~PwmDigitalOut() = default;

  /// Sets the output to be driven by the PWM block.
  void Enable() { DoEnable(); }

  /// Resets the output to a default configuration.
  void Disable() { DoDisable(); }

  /// Sets the output level of the output.
  ///
  /// 0 is off, std::limits::max<uint16_t> is full on.
  void SetLevel(uint16_t level) { DoSetLevel(level); }

  /// Sets a callback to invoke periodically.
  ///
  /// The behavior of the callback may vary cyclically, i.e. fading an LED on
  /// and off. The callback is invoked a number of times in each interval of
  /// time.
  ///
  /// @param  callback      Function to invoke repeatedly.
  /// @param  per_interval  Number of time the function should be invoked per
  ///                       elaped interval.
  /// @param  interval_ms   The dureation of each interval, in milliseconds.
  void SetCallback(Callback&& callback,
                   uint16_t per_interval,
                   uint32_t interval_ms);

  /// Discards the previosuly set callback, if any.
  void ClearCallback();

 protected:
  PwmDigitalOut() = default;

 private:
  /// copydoc `PwmDigitalOut::Enable`.
  virtual void DoEnable() = 0;

  /// copydoc `PwmDigitalOut::Disable`.
  virtual void DoDisable() = 0;

  /// copydoc `PwmDigitalOut::SetLevel`.
  virtual void DoSetLevel(uint16_t level) = 0;

  /// copydoc `PwmDigitalOut::SetCallback`.
  virtual void DoSetCallback(const Callback& callback,
                             uint16_t per_interval,
                             pw::chrono::SystemClock::duration interval) = 0;

  /// copydoc `PwmDigitalOut::ClearCallback`.
  virtual void DoClearCallback() = 0;

  std::optional<Callback> callback_;
};

}  // namespace sense

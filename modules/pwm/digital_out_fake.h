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

#include "modules/pwm/digital_out.h"
#include "pw_chrono/system_clock.h"
#include "pw_sync/thread_notification.h"
#include "pw_sync/timed_thread_notification.h"

namespace sense {

/// This class represents an fake output being driven by the PWM block.
///
/// On host, there is no PWM block, so this portable alternative logs
/// has methods that logs their parameters.
class PwmDigitalOutFake : public PwmDigitalOut {
 public:
  using Notification = ::pw::sync::ThreadNotification;

  PwmDigitalOutFake() = default;

  bool enabled() const { return enabled_; }
  uint16_t level() const { return level_; }

  /// Enables or disables "synchronous mode".
  ///
  /// When enabled, each call to `SetLevel` will block until another thread
  /// calls `Await`, `TryAwait`, or `TryAwaitUntil`.
  void set_sync(bool sync) { sync_ = sync; }

  /// Blocks until a call to `SetLevel` has been made.
  void Await();

  /// Returns whether a call to `SetLevel` has been made.
  bool TryAwait();

  /// Returns whether `SetLevel` is called before the given expiration.
  bool TryAwaitUntil(pw::chrono::SystemClock::time_point expiration);

 private:
  void DoEnable() override;

  void DoDisable() override;

  void DoSetLevel(uint16_t level) override;

  void DoSetCallback(uint16_t per_interval,
                     pw::chrono::SystemClock::duration interval) override;

  void DoClearCallback() override;

  bool enabled_ = false;
  uint16_t level_ = 0;
  bool sync_ = false;
  pw::sync::TimedThreadNotification notify_;
  pw::sync::ThreadNotification ack_;
  ;
};

}  // namespace sense

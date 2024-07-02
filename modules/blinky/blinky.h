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

#include "modules/indicators/system_led.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono/system_timer.h"
#include "pw_function/function.h"
#include "pw_status/status.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"
#include "pw_work_queue/work_queue.h"

namespace am {

/// Simple component that blink the on-board LED.
class Blinky final {
 public:
  constexpr static pw::chrono::SystemClock::duration kDefaultInterval =
      std::chrono::seconds(1);

  Blinky(pw::work_queue::WorkQueue& work_queue, SystemLed& led)
      : work_queue_(work_queue),
        led_(led),
        timer_(pw::bind_member<&Blinky::ToggleCallback>(this)) {}

  /// Turns the LED on if it is off, and off if it is on.
  pw::Status Toggle() PW_LOCKS_EXCLUDED(lock_);

  /// Queues a sequence of call backs to blink the configured number of times.
  ///
  /// @param  blink_count   The number of times to blink the LED.
  /// @param  interval_ms   The duration of a blink, in milliseconds.
  pw::Status Blink(uint32_t blink_count, uint32_t interval_ms)
      PW_LOCKS_EXCLUDED(lock_);

  /// Returns whether this instance is currently blinking or not.
  bool IsIdle() const PW_LOCKS_EXCLUDED(lock_);

 private:
  /// Adds a toggle callback to the work queue.
  pw::Status ScheduleToggleLocked() PW_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  /// Callback for the timer to toggle the LED.
  void ToggleCallback(pw::chrono::SystemClock::time_point);

  pw::work_queue::WorkQueue& work_queue_;
  SystemLed& led_;
  pw::chrono::SystemTimer timer_;

  mutable pw::sync::InterruptSpinLock lock_;
  uint32_t num_toggles_ PW_GUARDED_BY(lock_) =
      std::numeric_limits<uint32_t>::max();
  pw::chrono::SystemClock::duration interval_ PW_GUARDED_BY(lock_) =
      kDefaultInterval;
};

}  // namespace am

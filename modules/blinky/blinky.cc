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
#define PW_LOG_MODULE_NAME "BLINKY"

#include "modules/blinky/blinky.h"

#include <mutex>

#include "pw_log/log.h"
#include "pw_preprocessor/compiler.h"
#include "pw_status/try.h"

namespace am {

pw::Status Blinky::Toggle() {
  std::lock_guard lock(lock_);
  timer_.Cancel();
  PW_LOG_INFO("Toggling LED");
  led_.Toggle();
  if (num_toggles_ > 0) {
    --num_toggles_;
  }
  return ScheduleToggleLocked();
}

void Blinky::ToggleCallback(pw::chrono::SystemClock::time_point) {
  Toggle().IgnoreError();
}

pw::Status Blinky::Blink(uint32_t blink_count, uint32_t interval_ms) {
  std::lock_guard lock(lock_);
  timer_.Cancel();
  led_.TurnOff();

  // Multiply by two as each blink is an on/off pair.
  if (PW_MUL_OVERFLOW(blink_count, 2, &num_toggles_)) {
    num_toggles_ = 0;
  }
  if (num_toggles_ == 0) {
    num_toggles_ = std::numeric_limits<uint32_t>::max();
  }

  interval_ = pw::chrono::SystemClock::for_at_least(
      std::chrono::milliseconds(interval_ms));

  if (num_toggles_ == std::numeric_limits<uint32_t>::max()) {
    PW_LOG_INFO("Blinking forever at a %ums interval", interval_ms);
  } else {
    PW_LOG_INFO(
        "Blinking %u times at a %ums interval", num_toggles_ / 2, interval_ms);
  }
  return ScheduleToggleLocked();
}

bool Blinky::IsIdle() const {
  std::lock_guard lock(lock_);
  return num_toggles_ > 0;
}

pw::Status Blinky::ScheduleToggleLocked() {
  // Scheduling the timer again might not be safe from this context, so instead
  // just defer to the work queue.
  if (num_toggles_ > 0) {
    PW_TRY(work_queue_.PushWork([this]() {
      std::lock_guard lock(lock_);
      timer_.InvokeAfter(interval_);
    }));
  } else {
    PW_LOG_INFO("Stopped blinking");
  }
  return pw::OkStatus();
}

}  // namespace am

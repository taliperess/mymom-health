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
#include "blinky.h"
#define PW_LOG_MODULE_NAME "BLINKY"

#include <mutex>

#include "modules/blinky/blinky.h"
#include "modules/worker/worker.h"
#include "pw_log/log.h"
#include "pw_preprocessor/compiler.h"
#include "pw_status/try.h"
#include "pw_system/system.h"
#include "system/system.h"

namespace am {

Blinky::Blinky() : timer_(pw::bind_member<&Blinky::ToggleCallback>(this)) {}

void Blinky::Init(Worker& worker, MonochromeLed& monochrome_led) {
  worker_ = &worker;
  monochrome_led_ = &monochrome_led;
}

Blinky::~Blinky() { timer_.Cancel(); }

pw::chrono::SystemClock::duration Blinky::interval() const {
  std::lock_guard lock(lock_);
  return interval_;
}

pw::Status Blinky::Toggle() {
  timer_.Cancel();
  PW_LOG_INFO("Toggling LED");
  {
    std::lock_guard lock(lock_);
    monochrome_led_->Toggle();
    if (num_toggles_ > 0) {
      --num_toggles_;
    }
  }
  return ScheduleToggle();
}

void Blinky::ToggleCallback(pw::chrono::SystemClock::time_point) {
  Toggle().IgnoreError();
}

pw::Status Blinky::Blink(uint32_t blink_count, uint32_t interval_ms) {
  uint32_t num_toggles;
  if (PW_MUL_OVERFLOW(blink_count, 2, &num_toggles)) {
    num_toggles = 0;
  }
  if (num_toggles == 0) {
    PW_LOG_INFO("Blinking forever at a %ums interval", interval_ms);
    num_toggles = std::numeric_limits<uint32_t>::max();
  } else {
    PW_LOG_INFO(
        "Blinking %u times at a %ums interval", num_toggles / 2, interval_ms);
  }
  pw::chrono::SystemClock::duration interval =
      pw::chrono::SystemClock::for_at_least(
          std::chrono::milliseconds(interval_ms));

  timer_.Cancel();
  {
    std::lock_guard lock(lock_);
    monochrome_led_->TurnOff();
    num_toggles_ = num_toggles;
    interval_ = interval;
  }
  return ScheduleToggle();
}

void Blinky::Pulse(uint32_t interval_ms) {
  std::lock_guard lock(lock_);
  monochrome_led_->Pulse(interval_ms);
}

bool Blinky::IsIdle() const {
  std::lock_guard lock(lock_);
  return num_toggles_ == 0;
}

pw::Status Blinky::ScheduleToggle() {
  uint32_t num_toggles;
  {
    std::lock_guard lock(lock_);
    num_toggles = num_toggles_;
  }
  // Scheduling the timer again might not be safe from this context, so instead
  // just defer to the work queue.
  if (num_toggles > 0) {
    worker_->RunOnce([this]() { timer_.InvokeAfter(interval()); });
  } else {
    PW_LOG_INFO("Stopped blinking");
  }
  return pw::OkStatus();
}

}  // namespace am

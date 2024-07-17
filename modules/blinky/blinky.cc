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

#include "pw_log/log.h"
#include "pw_preprocessor/compiler.h"

#include "modules/blinky/blinky.h"
#include "modules/worker/worker.h"

namespace am {

Blinky::Blinky() : timer_(pw::bind_member<&Blinky::ToggleCallback>(this)) {}

void Blinky::Init(Worker& worker,
                  MonochromeLed& monochrome_led,
                  PolychromeLed& polychrome_led) {
  worker_ = &worker;

  monochrome_led_ = &monochrome_led;
  monochrome_led_->TurnOff();

  polychrome_led_ = &polychrome_led;
  polychrome_led_->TurnOff();
}

Blinky::~Blinky() { timer_.Cancel(); }

pw::chrono::SystemClock::duration Blinky::interval() const {
  std::lock_guard lock(lock_);
  return interval_;
}

void Blinky::Toggle() {
  timer_.Cancel();
  PW_LOG_INFO("Toggling LED");
  {
    std::lock_guard lock(lock_);
    monochrome_led_->Toggle();
    if (num_toggles_ > 0) {
      --num_toggles_;
    }
  }
}

void Blinky::SetLed(bool on) {
  timer_.Cancel();
  std::lock_guard lock(lock_);
  if (on) {
    PW_LOG_INFO("Setting LED on");
    monochrome_led_->TurnOn();
  } else {
    PW_LOG_INFO("Setting LED off");
    monochrome_led_->TurnOff();
  }
}

void Blinky::ToggleCallback(pw::chrono::SystemClock::time_point) {
  Toggle();
  return ScheduleToggle().IgnoreError();
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
  timer_.Cancel();
  PW_LOG_INFO("Pulsing forever at a %ums interval", interval_ms);
  std::lock_guard lock(lock_);
  monochrome_led_->Pulse(interval_ms);
}

void Blinky::SetRgb(uint8_t red,
                    uint8_t green,
                    uint8_t blue,
                    uint8_t brightness) {
  timer_.Cancel();
  PW_LOG_INFO("Setting RGB LED with red=0x%02x, green=0x%02x, blue=0x%02x",
              red,
              green,
              blue);
  std::lock_guard lock(lock_);
  polychrome_led_->SetColor(red, green, blue);
  polychrome_led_->SetBrightness(brightness);
  polychrome_led_->TurnOn();
}

void Blinky::Rainbow(uint32_t interval_ms) {
  timer_.Cancel();
  PW_LOG_INFO("Cycling through rainbow at a %ums interval", interval_ms);
  std::lock_guard lock(lock_);
  polychrome_led_->Rainbow(interval_ms);
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

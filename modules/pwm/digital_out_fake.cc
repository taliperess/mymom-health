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

#include "modules/pwm/digital_out_fake.h"

#include <chrono>

#include "pw_log/log.h"

namespace sense {

void PwmDigitalOutFake::Await() {
  if (sync_) {
    notify_.acquire();
    ack_.release();
  }
}

bool PwmDigitalOutFake::TryAwait() {
  if (sync_ && notify_.try_acquire()) {
    ack_.release();
    return true;
  }
  return false;
}

bool PwmDigitalOutFake::TryAwaitUntil(
    pw::chrono::SystemClock::time_point expiration) {
  if (sync_ && notify_.try_acquire_until(expiration)) {
    ack_.release();
    return true;
  }
  return false;
}

void PwmDigitalOutFake::DoEnable() {
  enabled_ = true;
  PW_LOG_INFO("PWM: +");
}

void PwmDigitalOutFake::DoDisable() {
  enabled_ = false;
  PW_LOG_INFO("PWM: -");
}

void PwmDigitalOutFake::DoSetLevel(uint16_t level) {
  level_ = level;
  if (sync_) {
    notify_.release();
    ack_.acquire();
  }
}

void PwmDigitalOutFake::DoSetCallback(uint16_t,
                                      pw::chrono::SystemClock::duration) {}

void PwmDigitalOutFake::DoClearCallback() {}

}  // namespace sense

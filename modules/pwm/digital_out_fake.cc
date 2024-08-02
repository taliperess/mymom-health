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

void PwmDigitalOutFake::DoEnable() { PW_LOG_INFO("PWM: +"); }

void PwmDigitalOutFake::DoDisable() { PW_LOG_INFO("PWM: -"); }

void PwmDigitalOutFake::DoSetLevel(uint16_t level) {
  PW_LOG_INFO("PWM: level=%u", level);
}

void PwmDigitalOutFake::DoSetCallback(
    uint16_t per_interval, pw::chrono::SystemClock::duration interval) {
  uint32_t ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(interval).count();
  PW_LOG_INFO(
      "PWM: callback to be invoked %u times per %u ms", per_interval, ms);
}

void PwmDigitalOutFake::DoClearCallback() {
  PW_LOG_INFO("PWM: callback cleared");
}

}  // namespace sense

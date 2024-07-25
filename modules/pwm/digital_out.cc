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

#include "modules/pwm/digital_out.h"

namespace sense {

void PwmDigitalOut::SetCallback(Callback&& callback,
                                uint16_t per_interval,
                                uint32_t interval_ms) {
  callback_ = std::move(callback);
  DoSetCallback(callback_.value(),
                per_interval,
                pw::chrono::SystemClock::for_at_least(
                    std::chrono::milliseconds(interval_ms)));
};

void PwmDigitalOut::ClearCallback() {
  callback_.reset();
  DoClearCallback();
}

}  // namespace sense

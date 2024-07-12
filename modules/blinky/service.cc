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

#include "modules/blinky/service.h"

namespace am {

void BlinkyService::Init(Worker& worker, MonochromeLed& led) {
  blinky_.Init(worker, led);
}

pw::Status BlinkyService::ToggleLed(const pw_protobuf_Empty&,
                                    pw_protobuf_Empty&) {
  return blinky_.Toggle();
}

pw::Status BlinkyService::Blink(const blinky_BlinkRequest& request,
                                pw_protobuf_Empty&) {
  uint32_t interval_ms = request.interval_ms == 0 ? Blinky::kDefaultIntervalMs
                                                  : request.interval_ms;
  uint32_t blink_count = request.has_blink_count ? request.blink_count : 0;
  return blinky_.Blink(blink_count, interval_ms);
}

}  // namespace am

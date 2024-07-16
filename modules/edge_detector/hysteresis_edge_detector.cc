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

#define PW_LOG_LEVEL PW_LOG_LEVEL_INFO

#include "modules/edge_detector/hysteresis_edge_detector.h"

#include "pw_log/log.h"

namespace am::internal {

Edge BaseHysteresisEdgeDetector::UpdateState(Event event) {
  switch (state_) {
    case kInitial:
      if (event == kLowSample) {
        state_ = kLow;
      } else if (event == kHighSample) {
        state_ = kHigh;
      }
      break;
    case kLow:
      if (event == kHighSample) {
        PW_LOG_DEBUG("EdgeDetector %p: rising edge detected", this);
        state_ = kHigh;
        return Edge::kRising;
      }
      break;
    case kHigh:
      if (event == kLowSample) {
        PW_LOG_DEBUG("EdgeDetector %p: falling edge detected", this);
        state_ = kLow;
        return Edge::kFalling;
      }
      break;
  }
  return Edge::kNone;
}

}  // namespace am::internal

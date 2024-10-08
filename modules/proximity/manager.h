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

#include "modules/edge_detector/pubsub.h"
#include "modules/pubsub/pubsub_events.h"

namespace sense {

class ProximityManager {
 public:
  /// Reports near/far proximity events through PubSub. Uses the provided
  /// thresholds, which are in unspecified units ranging from 0 (farthest) to
  /// 65535 (nearest).
  ProximityManager(PubSub& pubsub,
                   uint16_t inactive_threshold,
                   uint16_t active_threshold);

 private:
  struct ProxSamplerPubSub {
    using PubSub = sense::PubSub;
    using Sample = uint16_t;
    using SampleEvent = ProximitySample;

    static uint16_t GetSample(SampleEvent event) { return event.sample; }

    static ProximityStateChange GetEvent(Edge edge) {
      return {edge == Edge::kRising};
    }
  };

  PubSubHysteresisEdgeDetector<ProxSamplerPubSub> edge_detector_;
};

}  // namespace sense

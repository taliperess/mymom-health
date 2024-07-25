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

#include "modules/edge_detector/hysteresis_edge_detector.h"
#include "modules/pubsub/pubsub.h"
#include "pw_assert/assert.h"

namespace sense {

/// EdgeDetector that subscribes to sample events and publishes edges.
///
///   - `PubSub`: Type of the PubSub object (`sense::GenericPubSub<EventType>`)
///   - `Sample`: The sample type (e.g. `uint16_t`).
///   - `SampleEvent`: Event type to subscribe to for samples.
///   - `GetSample(SampleEvent)`: Returns the sample from the sample event.
///   - `GetEvent(Edge)`: Returns event to publish when an edge is detected.
template <typename PubSubSamplerMeta>
class PubSubHysteresisEdgeDetector
    : public HysteresisEdgeDetector<typename PubSubSamplerMeta::Sample> {
  using Sample = typename PubSubSamplerMeta::Sample;
  using PubSub = typename PubSubSamplerMeta::PubSub;
  using SampleEvent = typename PubSubSamplerMeta::SampleEvent;

 public:
  PubSubHysteresisEdgeDetector(PubSub& pubsub,
                               Sample inactive_threshold,
                               Sample active_threshold)
      : HysteresisEdgeDetector<Sample>(inactive_threshold, active_threshold),
        pubsub_(pubsub) {
    PW_ASSERT(
        pubsub_.template SubscribeTo<SampleEvent>([this](SampleEvent event) {
          Update(PubSubSamplerMeta::GetSample(event));
        }));
  }

 private:
  void Update(Sample sample) {
    switch (HysteresisEdgeDetector<Sample>::Update(sample)) {
      case Edge::kNone:
        break;
      case Edge::kRising:
        pubsub_.Publish(PubSubSamplerMeta::GetEvent(Edge::kRising));
        break;
      case Edge::kFalling:
        pubsub_.Publish(PubSubSamplerMeta::GetEvent(Edge::kFalling));
        break;
    }
  }

  PubSub& pubsub_;
};

}  // namespace sense

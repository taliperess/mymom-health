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

#include "modules/pubsub/pubsub_events.h"
#include "modules/pubsub/pubsub_pb/pubsub.rpc.pb.h"

namespace sense {

class PubSubService final
    : public ::pubsub::pw_rpc::nanopb::PubSub::Service<PubSubService> {
 public:
  void Init(PubSub& pubsub);

  pw::Status Publish(const pubsub_Event& request, pw_protobuf_Empty&);
  void Subscribe(const pw_protobuf_Empty&, ServerWriter<pubsub_Event>& writer);

 private:
  PubSub* pubsub_ = nullptr;
  ServerWriter<pubsub_Event> stream_;
};

}  // namespace sense

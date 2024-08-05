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

#include <optional>

#include "modules/pubsub/pubsub_events.h"
#include "modules/state_manager/state_manager.rpc.pb.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"

namespace sense {

class StateManagerService final
    : public ::state_manager::pw_rpc::nanopb::StateManager::Service<
          StateManagerService> {
 public:
  StateManagerService(PubSub& pubsub);

  pw::Status ChangeThreshold(
      const state_manager_ChangeThresholdRequest& request,
      pw_protobuf_Empty& response);
  pw::Status SilenceAlarm(const pw_protobuf_Empty&, pw_protobuf_Empty&);
  pw::Status GetState(const pw_protobuf_Empty&, state_manager_State& response);

 private:
  PubSub* pubsub_;
  pw::sync::InterruptSpinLock current_state_lock_;
  std::optional<SenseState> current_state_ PW_GUARDED_BY(current_state_lock_);
};

}  // namespace sense

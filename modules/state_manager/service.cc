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

#include "modules/state_manager/service.h"

#include <mutex>

#include "pw_string/util.h"

namespace sense {

StateManagerService::StateManagerService(PubSub& pubsub) : pubsub_(&pubsub) {
  pubsub_->SubscribeTo<SenseState>([this](SenseState event) {
    std::lock_guard lock(current_state_lock_);
    current_state_ = event;
  });
}

pw::Status StateManagerService::ChangeThreshold(
    const state_manager_ChangeThresholdRequest& request, pw_protobuf_Empty&) {
  bool success;

  if (request.increment) {
    success = pubsub_->Publish(
        StateManagerControl(StateManagerControl::kIncrementThreshold));
  } else {
    success = pubsub_->Publish(
        StateManagerControl(StateManagerControl::kDecrementThreshold));
  }

  return success ? pw::OkStatus() : pw::Status::Unavailable();
}

pw::Status StateManagerService::SilenceAlarm(const pw_protobuf_Empty&,
                                             pw_protobuf_Empty&) {
  bool success = pubsub_->Publish(
      StateManagerControl(StateManagerControl::kSilenceAlarms));
  return success ? pw::OkStatus() : pw::Status::Unavailable();
}

pw::Status StateManagerService::GetState(const pw_protobuf_Empty&,
                                         state_manager_State& response) {
  std::lock_guard lock(current_state_lock_);

  if (!current_state_.has_value()) {
    return pw::Status::Unavailable();
  }

  const auto& current_state = current_state_.value();
  response.alarm_active = current_state.alarm;
  response.alarm_threshold = current_state.alarm_threshold;
  response.aq_score = current_state.air_quality;
  return pw::string::Copy(current_state.air_quality_description,
                          response.aq_description)
      .status();
}

}  // namespace sense

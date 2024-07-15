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
#include "modules/buttons/manager.h"
#define PW_LOG_MODULE_NAME "BUTTONS"

#include "pw_status/try.h"

#define PW_LOG_MODULE_NAME "BUTTONS"

using pw::chrono::SystemClock;
using pw::digital_io::DigitalIn;
using pw::digital_io::State;

namespace am {
State Debouncer::UpdateState(SystemClock::time_point now, State state) {
  if (state != last_input_) {
    last_update_ = now;
    last_input_ = state;
  } else if (now - last_update_ >= kDebounceInterval) {
    output_ = state;
  }

  return output_;
}

EdgeDetector::StateChange EdgeDetector::UpdateState(State state) {
  auto prev_state = current_state_;
  current_state_ = state;

  if (prev_state == State::kInactive && current_state_ == State::kActive) {
    return StateChange::kActivate;
  }
  if (prev_state == State::kActive && current_state_ == State::kInactive) {
    return StateChange::kDeactivate;
  }
  return StateChange::kNone;
}

pw::Result<EdgeDetector::StateChange> Button::Sample(
    SystemClock::time_point now) {
  PW_TRY_ASSIGN(auto raw_state, io_.GetState());
  auto deboucned_state = debouncer_.UpdateState(now, raw_state);
  return edge_detector_.UpdateState(deboucned_state);
}

ButtonManager::ButtonManager(pw::digital_io::DigitalIn& button_a,
                pw::digital_io::DigitalIn& button_b,
                pw::digital_io::DigitalIn& button_x,
                pw::digital_io::DigitalIn& button_y) : buttons_{
                  Button(button_a),
                  Button(button_b),
                  Button(button_x),
                  Button(button_y),
                }
    , timer_(pw::bind_member<&ButtonManager::SampleCallback>(this)) {}

ButtonManager::~ButtonManager() {}

void ButtonManager::Init(PubSub& pub_sub, Worker& worker) {
  pub_sub_ = &pub_sub;
  worker_ = &worker;

  // Start the periodic sampling callbacks.
  timer_.InvokeAfter(kSampleInterval);
}

void ButtonManager::SampleCallback(SystemClock::time_point now) {
  PW_CHECK_NOTNULL(worker_);
  worker_->RunOnce([this, now]() {
    SampleButtons(now);
    // Start the periodic sampling callbacks.
    timer_.InvokeAfter(kSampleInterval);
  });
}

template <typename ButtonEvent>
pw::Status ButtonManager::SampleButton(Button& button,
                                       SystemClock::time_point now) {
  PW_TRY_ASSIGN(auto state_change, button.Sample(now));
  if (state_change == EdgeDetector::StateChange::kActivate) {
    pub_sub_->Publish(ButtonEvent(true));
  } else if (state_change == EdgeDetector::StateChange::kDeactivate) {
    pub_sub_->Publish(ButtonEvent(false));
  }
  return pw::OkStatus();
}

pw::Status ButtonManager::SampleButtons(SystemClock::time_point now) {
  PW_TRY(SampleButton<am::ButtonA>(buttons_[0], now));
  PW_TRY(SampleButton<am::ButtonB>(buttons_[1], now));
  PW_TRY(SampleButton<am::ButtonX>(buttons_[2], now));
  PW_TRY(SampleButton<am::ButtonY>(buttons_[3], now));
  return pw::OkStatus();
}

}  // namespace am

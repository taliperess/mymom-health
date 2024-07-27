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

#include "modules/state_manager/state_manager.h"

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_string/format.h"

namespace sense {

void StateManager::Init() {
  pubsub_->Subscribe([this](Event event) { Update(event); });
}

void StateManager::Update(Event event) {
  switch (static_cast<EventType>(event.index())) {
    case kAlarmStateChange:
    case kProximityStateChange:
    case kProximitySample:
      break;  // ignore these events
    case kButtonA:
      HandleButtonPress(std::get<ButtonA>(event).pressed(),
                        &State::ButtonAReleased);
      break;
    case kButtonB:
      HandleButtonPress(std::get<ButtonB>(event).pressed(),
                        &State::ButtonBReleased);
      break;
    case kButtonX:
      HandleButtonPress(std::get<ButtonX>(event).pressed(),
                        &State::ButtonXReleased);
      break;
    case kButtonY:
      HandleButtonPress(std::get<ButtonY>(event).pressed(),
                        &State::ButtonYReleased);
      break;
    case kLedValueColorRotationMode:
      state_.get().ColorRotationModeLedValue(
          std::get<LedValueColorRotationMode>(event));
      break;
    case kLedValueMorseCodeMode:
      state_.get().MorseCodeModeLedValue(
          std::get<LedValueMorseCodeMode>(event));
      break;
    case kLedValueProximityMode:
      state_.get().ProximityModeLedValue(
          std::get<LedValueProximityMode>(event));
      break;
    case kLedValueAirQualityMode:
      state_.get().AirQualityModeLedValue(
          std::get<LedValueAirQualityMode>(event));
      break;
    case kDemoModeTimerExpired:
      state_.get().DemoModeTimerExpired();
      break;
    case kAirQuality:
      last_air_quality_score_ = std::get<AirQuality>(event).score;
      break;
    case kMorseEncodeRequest:
      break;  // ignore these events
  }
}

void StateManager::HandleButtonPress(bool pressed, void (State::*function)()) {
  if (pressed) {
    led_.Override(0xffffff, 255);  // Bright white while pressed.
  } else {
    led_.EndOverride();
    (state_.get().*function)();
  }
}

void StateManager::StartMorseReadout() {
  pw::Status status = pw::string::FormatOverwrite(
      air_quality_score_string_, "%hu", last_air_quality_score_);
  PW_CHECK_OK(status);
  pubsub_->Publish(
      MorseEncodeRequest{.message = air_quality_score_string_, .repeat = 1});
}

void StateManager::MorseReadout::MorseCodeModeLedValue(
    LedValueMorseCodeMode value) {
  manager().led_.SetColor(value);
  if (value.pattern_finished()) {
    manager().SetState<AirQualityMode>();
  }
}

void StateManager::LogStateChange(const char* old_state) const {
  PW_LOG_INFO("StateManager: %s -> %s", old_state, state_.get().name());
}

}  // namespace sense

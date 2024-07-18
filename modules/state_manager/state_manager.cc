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

namespace am {
void StateManager::Init() {
  pubsub_->Subscribe([this](Event event) {
    if (std::holds_alternative<ButtonA>(event)) {
      HandleButtonPress(kProximity, std::get<ButtonA>(event).pressed());
    } else if (std::holds_alternative<ButtonB>(event)) {
      HandleButtonPress(kTemperature, std::get<ButtonB>(event).pressed());
    } else if (std::holds_alternative<ButtonX>(event)) {
      HandleButtonPress(kMorseCode, std::get<ButtonX>(event).pressed());
    } else if (std::holds_alternative<ButtonY>(event)) {
      HandleButtonPress(kColorRotation, std::get<ButtonY>(event).pressed());
    } else {
      HandleStateEvent(event);
    }
  });
}

void StateManager::HandleButtonPress(State button_state, bool pressed) {
  if (pressed) {
    state_ = kButtonPressed;
    led_->SetColor(0xff0000);
    led_->SetBrightness(255);
  } else {
    led_->SetColor(0xffffff);
    SetState(button_state);
  }
}

void StateManager::SetState(State state) {
  PW_CHECK(state != kButtonPressed);

  state_ = state;
  PW_LOG_INFO("State set to %s", StateString(state_));

  switch (state_) {
    case kProximity:
      led_->SetColor(0xffffff);
      break;
    case kTemperature:
      break;
    case kMorseCode:
      break;
    case kColorRotation:
      break;
    case kButtonPressed:
      PW_UNREACHABLE;
  }
}

void StateManager::HandleStateEvent(Event& event) {
  switch (state_) {
    case kProximity:
      if (std::holds_alternative<LedValueProximityMode>(event)) {
        SetLed(std::get<LedValueProximityMode>(event));
      }
      break;
    case kTemperature:
      if (std::holds_alternative<LedValueVocMode>(event)) {
        SetLed(std::get<LedValueVocMode>(event));
      }
      break;
    case kMorseCode:
      if (std::holds_alternative<LedValueMorseCodeMode>(event)) {
        SetLed(std::get<LedValueMorseCodeMode>(event));
      }
      break;
    case kColorRotation:
      if (std::holds_alternative<LedValueColorRotationMode>(event)) {
        SetLed(std::get<LedValueColorRotationMode>(event));
      }
      break;
    case kButtonPressed:
      // Ignore events in a transition state.
      return;
  }
}

void StateManager::HandleProximityEvent(ProximityStateChange& event) {
  if (event.proximity) {
    led_->SetBrightness(255);
  } else {
    led_->SetBrightness(64);
  }
}

const char* StateManager::StateString(State state) {
  switch (state) {
    case kProximity:
      return "PROXIMITY";
    case kTemperature:
      return "TEMPERATURE";
    case kMorseCode:
      return "MORSE_CODE";
    case kColorRotation:
      return "COLOR_ROTATION";
    case kButtonPressed:
      return "BUTTON_PRESSED";
  }

  return "UNKNOWN";
}

}  // namespace am
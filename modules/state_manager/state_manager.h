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

#include "modules/led/polychrome_led.h"
#include "modules/pubsub/pubsub_events.h"

namespace sense {

class StateManager {
 public:
  StateManager(PubSub& pubsub, PolychromeLed& led)
      : pubsub_(&pubsub), led_(&led), state_(kColorRotation) {}

  void Init();

 private:
  enum State {
    kProximity,
    kTemperature,
    kMorseCode,
    kColorRotation,
    kButtonPressed,
  };

  void HandleButtonPress(State button_state, bool pressed);
  void SetState(State state);
  void SetLed(LedValue value) {
    led_->SetColor(value.r(), value.g(), value.b());
  }

  void HandleStateEvent(Event& event);
  void HandleProximityEvent(ProximityStateChange& event);

  static const char* StateString(State state);

  PubSub* pubsub_;
  PolychromeLed* led_;
  State state_;
};

}  // namespace sense

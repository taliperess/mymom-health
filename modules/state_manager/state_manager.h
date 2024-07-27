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
#include "modules/state_manager/common_base_union.h"
#include "pw_chrono/system_timer.h"
#include "pw_string/string.h"

namespace sense {

// State machine that controls what displays on the LED.
class LedOutputStateMachine {
 public:
  explicit constexpr LedOutputStateMachine(PolychromeLed& led,
                                           uint8_t brightness)
      : state_(kPassthrough),
        brightness_(brightness),
        red_(0),
        green_(0),
        blue_(0),
        led_(&led) {}

  void Override(uint32_t color, uint8_t brightness) {
    state_ = kOverride;
    led_->SetColor(color);
    led_->SetBrightness(brightness);
  }

  void EndOverride() {
    state_ = kPassthrough;
    UpdateLed();
  }

  void SetColor(const LedValue& value) {
    red_ = value.r();
    green_ = value.g();
    blue_ = value.b();
    UpdateLed();
  }

  void SetBrightness(uint8_t brightness) {
    brightness_ = brightness;
    UpdateLed();
  }

 private:
  void UpdateLed() {
    if (state_ == kPassthrough) {
      led_->SetColor(red_, green_, blue_);
      led_->SetBrightness(brightness_);
    }
  }
  enum : bool {
    kPassthrough,  // show stored values and update as they come in
    kOverride,     // display a specific color
  } state_;

  uint8_t brightness_;
  uint8_t red_;
  uint8_t green_;
  uint8_t blue_;

  PolychromeLed* led_;
};

class StateManager {
 public:
  StateManager(PubSub& pubsub, PolychromeLed& led)
      : pubsub_(&pubsub),
        led_(led, kDefaultBrightness),
        state_(*this),
        demo_mode_timer_(
            [this](auto) { pubsub_->Publish(DemoModeTimerExpired{}); }) {}

  void Init();

 private:
  /// How long to show demo modes before returning to the regular AQI monitor.
  static constexpr pw::chrono::SystemClock::duration kDemoModeTimeout =
      std::chrono::seconds(30);

  static constexpr uint8_t kDefaultBrightness = 220;

  // Represents a state in the Sense app state machine.
  class State {
   public:
    constexpr State(StateManager& manager, const char* name)
        : manager_(manager), name_(name) {}

    virtual ~State() = default;

    // Name of the state for logging.
    const char* name() const { return name_; }

    // Events for releasing buttons.
    virtual void ButtonAReleased() {}
    virtual void ButtonBReleased() {}
    virtual void ButtonXReleased() {}
    virtual void ButtonYReleased() {}

    // Events for requested LED values from other components.
    virtual void ProximityModeLedValue(const LedValue&) {}
    virtual void AirQualityModeLedValue(const LedValue&) {}
    virtual void MorseCodeModeLedValue(LedValueMorseCodeMode) {}
    virtual void ColorRotationModeLedValue(const LedValue&) {}

    // Demo mode returns to air quality mode after a specified time.
    virtual void DemoModeTimerExpired() {}

   protected:
    StateManager& manager() { return manager_; }

   private:
    StateManager& manager_;
    const char* name_;
  };

  class AirQualityMode final : public State {
   public:
    constexpr AirQualityMode(StateManager& manager)
        : State(manager, "AirQualityMode") {}

    void ButtonXReleased() override { manager().SetState<ProximityDemo>(); }
    void ButtonYReleased() override { manager().SetState<MorseReadout>(); }

    void AirQualityModeLedValue(const LedValue& value) override {
      manager().led_.SetColor(value);
    }
  };

  class MorseReadout final : public State {
   public:
    MorseReadout(StateManager& manager) : State(manager, "MorseReadout") {
      manager.StartMorseReadout();
    }

    void ButtonXReleased() override { manager().SetState<ProximityDemo>(); }
    void ButtonYReleased() override { manager().SetState<AirQualityMode>(); }

    void MorseCodeModeLedValue(LedValueMorseCodeMode value) override;
  };

  // Base class for all demo states to handle button behavior and timer.
  class DemoState : public State {
   public:
    DemoState(StateManager& manager, const char* name) : State(manager, name) {
      manager.demo_mode_timer_.InvokeAfter(kDemoModeTimeout);
    }

    void ButtonYReleased() override { manager().SetState<MorseReadout>(); }

    void DemoModeTimerExpired() override {
      manager().SetState<AirQualityMode>();
    }
  };

  class ProximityDemo final : public DemoState {
   public:
    ProximityDemo(StateManager& manager)
        : DemoState(manager, "ProximityDemo") {}

    void ButtonXReleased() override { manager().SetState<MorseCodeDemo>(); }

    void ProximityModeLedValue(const LedValue& value) override {
      manager().led_.SetColor(value);
    }
  };

  class MorseCodeDemo final : public DemoState {
   public:
    MorseCodeDemo(StateManager& manager) : DemoState(manager, "MorseCodeDemo") {
      manager.pubsub_->Publish(
          MorseEncodeRequest{.message = "PW", .repeat = 0});
    }

    void ButtonXReleased() override { manager().SetState<ColorRotationDemo>(); }

    void MorseCodeModeLedValue(LedValueMorseCodeMode value) override {
      manager().led_.SetColor(value);
    }
  };

  class ColorRotationDemo final : public DemoState {
   public:
    ColorRotationDemo(StateManager& manager)
        : DemoState(manager, "ColorRotationDemo") {}

    void ButtonXReleased() override { manager().SetState<ProximityDemo>(); }

    void ColorRotationModeLedValue(const LedValue& value) override {
      manager().led_.SetColor(value);
    }
  };

  // Respond to a PubSub event.
  void Update(Event event);

  void HandleButtonPress(bool pressed, void (State::*function)());

  template <typename StateType>
  void SetState() {
    demo_mode_timer_.Cancel();  // always reset the timer
    const char* old_state = state_.get().name();
    state_.emplace<StateType>(*this);
    LogStateChange(old_state);
  }

  void LogStateChange(const char* old_state) const;

  void StartMorseReadout();

  PubSub* pubsub_;
  LedOutputStateMachine led_;

  CommonBaseUnion<State,
                  AirQualityMode,
                  MorseReadout,
                  ProximityDemo,
                  MorseCodeDemo,
                  ColorRotationDemo>
      state_;

  pw::chrono::SystemTimer demo_mode_timer_;

  uint16_t last_air_quality_score_ = 768;
  pw::InlineString<4> air_quality_score_string_;
};

}  // namespace sense

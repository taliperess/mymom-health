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

#include <cmath>

#include "modules/air_sensor/air_sensor.h"
#include "modules/led/polychrome_led.h"
#include "modules/pubsub/pubsub_events.h"
#include "modules/state_manager/common_base_union.h"
#include "modules/stats/simple_moving_average.h"
#include "pw_chrono/system_timer.h"
#include "pw_string/string.h"

namespace sense {

// State machine that controls what displays on the LED.
class LedOutputStateMachine {
 public:
  explicit LedOutputStateMachine(PolychromeLed& led, uint8_t brightness)
      : state_(kPassthrough),
        brightness_(brightness),
        red_(0),
        green_(0),
        blue_(0),
        led_(&led) {
    // Set the brightness, but start the color as black.
    led_->SetBrightness(brightness_);
    led_->SetColor(0);
  }

  // Use this color and brightness until EndOverride is called.
  void Override(uint32_t color, uint8_t brightness) {
    state_ = kOverride;
    led_->SetColor(color);
    led_->SetBrightness(brightness);
  }

  // Switch to the most recently set color and brightness.
  void EndOverride() {
    state_ = kPassthrough;
    led_->SetColor(red_, green_, blue_);
    led_->SetBrightness(brightness_);
  }

  void SetColor(const LedValue& value) {
    UpdateLed(value.r(), value.g(), value.b(), brightness_);
  }

  void SetBrightness(uint8_t brightness) {
    UpdateLed(red_, green_, blue_, brightness);
  }

 private:
  void UpdateLed(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness);

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

// Manages state for the "production" Sense app.
//
// This class is NOT thread safe. Must only be interacted with from the PubSub
// thread.
class StateManager {
 public:
  StateManager(PubSub& pubsub, PolychromeLed& led)
      : pubsub_(&pubsub),
        led_(led, brightness_),
        state_(*this),
        demo_mode_timer_(
            [this](auto) { pubsub_->Publish(DemoModeTimerExpired{}); }) {}

  void Init();

 private:
  /// How long to show demo modes before returning to the regular AQI monitor.
  static constexpr pw::chrono::SystemClock::duration kDemoModeTimeout =
      std::chrono::seconds(30);

  // LED brightness varies based on ambient light readings.
  static constexpr uint8_t kMinBrightness = 20;
  static constexpr uint8_t kDefaultBrightness = 160;
  static constexpr uint8_t kMaxBrightness = 255;

  static constexpr uint16_t kThresholdIncrement = 128;
  static constexpr uint16_t kMaxThreshold = 768;

  // Represents a state in the Sense app state machine.
  class State {
   public:
    State(StateManager& manager, const char* name)
        : manager_(manager), name_(name) {
      manager_.led_.SetBrightness(manager_.brightness_);
    }

    virtual ~State() = default;

    // Name of the state for logging.
    const char* name() const { return name_; }

    // Events for handling alarms.
    virtual void AlarmStateChanged(bool alarm) {
      if (manager_.alarmed_ == alarm) {
        return;
      }
      manager_.alarmed_ = alarm;
      if (alarm) {
        manager_.SetState<AirQualityAlarmMode>();
      } else {
        manager_.SetState<AirQualityMode>();
      }
    }

    // Events for releasing buttons.
    virtual void ButtonAReleased() {
      manager_.SetState<AirQualityThresholdMode>();
    }
    virtual void ButtonBReleased() {
      manager_.SetState<AirQualityThresholdMode>();
    }
    virtual void ButtonXReleased() {}
    virtual void ButtonYReleased() {}

    // Ambient light sensor updates determine LED brightess by default.
    virtual void AmbientLightUpdate() {
      manager().UpdateBrightnessFromAmbientLight();
    }

    // Events for requested LED values from other components.
    virtual void AirQualityModeLedValue(const LedValue&) {}

    virtual void MorseCodeEdge(const MorseCodeValue&) {}

    // Demo mode returns to air quality mode after a specified time.
    virtual void DemoModeTimerExpired() {}

   protected:
    StateManager& manager() { return manager_; }

   private:
    StateManager& manager_;
    const char* name_;
  };

  // Base class for all demo states to handle button behavior and timer.
  class TimeoutState : public State {
   public:
    TimeoutState(StateManager& manager,
                 const char* name,
                 pw::chrono::SystemClock::duration timeout)
        : State(manager, name) {
      manager.demo_mode_timer_.InvokeAfter(timeout);
    }

    void ButtonYReleased() override { manager().SetState<MorseReadout>(); }

    void DemoModeTimerExpired() override {
      manager().SetState<AirQualityMode>();
    }
  };

  class AirQualityMode final : public State {
   public:
    AirQualityMode(StateManager& manager) : State(manager, "AirQualityMode") {}

    void ButtonYReleased() override { manager().SetState<MorseReadout>(); }

    void AirQualityModeLedValue(const LedValue& value) override {
      manager().led_.SetColor(value);
    }
  };

  class AirQualityThresholdMode final : public TimeoutState {
   public:
    static constexpr auto kThresholdModeTimeout =
        pw::chrono::SystemClock::for_at_least(std::chrono::seconds(3));

    AirQualityThresholdMode(StateManager& manager)
        : TimeoutState(
              manager, "AirQualityThresholdMode", kThresholdModeTimeout) {
      manager.DisplayThreshold();
    }

    void ButtonAReleased() override {
      manager().IncrementThreshold(kThresholdModeTimeout);
    }

    void ButtonBReleased() override {
      manager().DecrementThreshold(kThresholdModeTimeout);
    }
  };

  class AirQualityAlarmMode final : public State {
   public:
    AirQualityAlarmMode(StateManager& manager)
        : State(manager, "AirQualityAlarmMode") {
      manager.StartMorseReadout(/* repeat: */ true);
    }

    void ButtonYReleased() override {
      manager().pubsub_->Publish(AlarmSilenceRequest{.seconds = 60});
    }
    void AirQualityModeLedValue(const LedValue& value) override {
      manager().led_.SetColor(value);
    }
    void MorseCodeEdge(const MorseCodeValue& value) override {
      manager().led_.SetBrightness(value.turn_on ? manager().brightness_ : 0);
    }
  };

  class MorseReadout final : public State {
   public:
    MorseReadout(StateManager& manager) : State(manager, "MorseReadout") {
      manager.StartMorseReadout(/* repeat: */ false);
    }

    void ButtonYReleased() override { manager().SetState<AirQualityMode>(); }

    void AirQualityModeLedValue(const LedValue& value) override {
      manager().led_.SetColor(value);
    }
    void MorseCodeEdge(const MorseCodeValue& value) override {
      manager().led_.SetBrightness(value.turn_on ? manager().brightness_ : 0);
      if (value.message_finished) {
        manager().SetState<AirQualityMode>();
      }
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

  void StartMorseReadout(bool repeat);

  void DisplayThreshold();

  void IncrementThreshold(pw::chrono::SystemClock::duration timeout);

  void DecrementThreshold(pw::chrono::SystemClock::duration timeout);

  void UpdateAverageAmbientLight(float ambient_light_sample_lux);

  // Recalculates the brightness level when the ambient light changes.
  void UpdateBrightnessFromAmbientLight();

  bool alarmed_ = false;
  IntegerSimpleMovingAverager<uint16_t, 5> prox_samples_;
  float ambient_light_lux_ = NAN;  // exponential moving averaged mean lux
  uint8_t brightness_ = kDefaultBrightness;
  uint16_t current_threshold_ = AirSensor::kDefaultTheshold;
  uint16_t last_air_quality_score_ = AirSensor::kAverageScore;
  pw::InlineString<4> air_quality_score_string_;

  PubSub* pubsub_;
  LedOutputStateMachine led_;

  CommonBaseUnion<State,
                  AirQualityMode,
                  AirQualityThresholdMode,
                  AirQualityAlarmMode,
                  MorseReadout>
      state_;

  pw::chrono::SystemTimer demo_mode_timer_;
};

}  // namespace sense

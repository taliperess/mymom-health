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
#include "modules/edge_detector/hysteresis_edge_detector.h"
#include "modules/led/polychrome_led.h"
#include "modules/pubsub/pubsub_events.h"
#include "modules/state_manager/common_base_union.h"
#include "pw_string/string.h"

namespace sense {

// Wraps a PolychromeLed and sets brightness from ambient light readings.
class AmbientLightAdjustedLed {
 public:
  AmbientLightAdjustedLed(PolychromeLed& led);

  void SetColor(const LedValue& color) {
    led_.SetColor(color.r(), color.g(), color.b());
  }

  void SetOnOff(bool turn_on) { led_.SetOnOff(turn_on); }

  // Recalculates the brightness level when the ambient light changes.
  void UpdateBrightnessFromAmbientLight(float ambient_light_sample_lux);

 private:
  static constexpr uint8_t kMinBrightness = 10;
  static constexpr uint8_t kDefaultBrightness = 160;
  static constexpr uint8_t kMaxBrightness = 255;

  void UpdateAverageAmbientLight(float ambient_light_sample_lux);

  PolychromeLed& led_;
  std::optional<float> ambient_light_lux_;
};

// Manages state for the "production" Sense app.
//
// This class is NOT thread safe. Must only be interacted with from the PubSub
// thread.
class StateManager {
 public:
  using TimerToken = ::pw::tokenizer::Token;

  static constexpr TimerToken kSilenceAlarmsToken =
      PW_TOKENIZE_STRING("re-enable alarms");
  static constexpr uint16_t kSilenceAlarmsTimeout = 60;

  static constexpr TimerToken kThresholdModeToken =
      PW_TOKENIZE_STRING("exit threshold mode");
  static constexpr uint16_t kThresholdModeTimeout = 3;

  static constexpr uint16_t kThresholdIncrement = 128;
  static constexpr uint16_t kMaxThreshold = 768;

  StateManager(PubSub& pubsub, PolychromeLed& led);

 private:
  // Represents a state in the Sense app state machine.
  class State {
   public:
    State(StateManager& manager, const char* name)
        : manager_(manager), name_(name) {}

    virtual ~State() = default;

    // Name of the state for logging.
    const char* name() const { return name_; }

    /// Button A enters `ThresholdMode` by default.
    virtual void ButtonAPressed() { manager_.SetState<ThresholdMode>(); }

    /// Button B enters `ThresholdMode` by default.
    virtual void ButtonBPressed() { manager_.SetState<ThresholdMode>(); }

    /// Button X enters resets the mode to either `MonitorMode` or `AlarmMode`
    /// by default, depending on the current air quality..
    virtual void ButtonXPressed() { manager_.ResetMode(); }

    /// Button Y enters `MorseReadoutMode` by default.
    virtual void ButtonYPressed() { manager().SetState<MorseReadoutMode>(); }

    // Update the LED color by default.
    virtual void OnLedValue(const LedValue& value) {
      manager().led_.SetColor(value);
    }

    // Ignore Morse code edges by default.
    virtual void MorseCodeEdge(const MorseCodeValue&) {}

    // Handles re-enabling alarms that were previously silenced.
    virtual void OnTimerExpired(const TimerExpired& timer) {
      if (timer.token == kSilenceAlarmsToken) {
        manager().alarm_silenced_ = false;
      }
    }

   protected:
    StateManager& manager() { return manager_; }

   private:
    StateManager& manager_;
    const char* name_;
  };

  /// Mode for monitoring the air quality.
  ///
  /// Inherits default button mapping.
  class MonitorMode final : public State {
   public:
    MonitorMode(StateManager& manager) : State(manager, "MonitorMode") {}
  };

  /// Mode for displaying and modifying the the air quality alarm threshold.
  ///
  /// Inherits default button mapping, except:
  ///  * Button A increments the threshold.
  ///  * Button B decrements the threshold.
  ///
  /// The mode will timeout and return to the default mode after 3 seconds of no
  /// button being pressed.
  class ThresholdMode final : public State {
   public:
    ThresholdMode(StateManager& manager) : State(manager, "ThresholdMode") {
      manager.DisplayThreshold();
    }

    void ButtonAPressed() override { manager().IncrementThreshold(); }

    void ButtonBPressed() override { manager().DecrementThreshold(); }

    void OnLedValue(const LedValue&) override {}

    void OnTimerExpired(const TimerExpired& timer) override {
      if (timer.token == kThresholdModeToken) {
        manager().ResetMode();
      } else {
        State::OnTimerExpired(timer);
      }
    }

    void ButtonXPressed() override { manager().SetState<MonitorMode>(); }

    void ButtonYPressed() override { manager().SetState<MonitorMode>(); }
  };

  /// Mode representing a triggered air quality alarm.
  ///
  /// Inherits default button mapping, except:
  ///  * Button X silences the alarm for 60 seconds.
  ///  * Button Y does nothing.
  class AlarmMode final : public State {
   public:
    AlarmMode(StateManager& manager) : State(manager, "AlarmMode") {
      manager.StartMorseReadout(/* repeat: */ true);
    }

    // Since morse code leaves the LED off, turn it back on.
    ~AlarmMode() { manager().led_.SetOnOff(true); }

    void ButtonXPressed() override { manager().SilenceAlarms(); }

    void ButtonYPressed() override {}

    void MorseCodeEdge(const MorseCodeValue& value) override {
      manager().led_.SetOnOff(value.turn_on);
    }
  };

  /// Mode that displays the current air quality in Morse code.
  ///
  /// Inherits default button mapping, except:
  ///  * Button Y restarts the air quality display.
  class MorseReadoutMode final : public State {
   public:
    MorseReadoutMode(StateManager& manager)
        : State(manager, "MorseReadoutMode") {
      manager.StartMorseReadout(/* repeat: */ false);
    }

    // Since morse code leaves the LED off, turn it back on.
    ~MorseReadoutMode() { manager().led_.SetOnOff(true); }

    void ButtonYPressed() override {
      manager().StartMorseReadout(/* repeat: */ false);
    }

    void MorseCodeEdge(const MorseCodeValue& value) override {
      manager().led_.SetOnOff(value.turn_on);
      if (value.message_finished) {
        manager().ResetMode();
      }
    }
  };

  /// Responds to a PubSub event.
  void Update(Event event);

  template <typename StateType>
  void SetState() {
    const char* old_state = state_.get().name();
    state_.emplace<StateType>(*this);
    LogStateChange(old_state);
  }

  /// Sets the state to `MonitorMode` or `AlarmMode`, depending on the current
  /// air quality.
  void ResetMode();

  /// Sets the LED to reflect the current alarm threshold.
  void DisplayThreshold();

  /// Increases the current alarm threshold.
  void IncrementThreshold();

  /// Decreases the current alarm threshold.
  void DecrementThreshold();

  /// Sets the current alarm threshold.
  void SetAlarmThreshold(uint16_t alarm_threshold);

  /// Incorporates a new air quality reading from the air sensor, changing the
  /// LED color and triggering alarms as appropriate.
  void UpdateAirQuality(uint16_t score);

  /// Suppresses `AlarmMode` for 60 seconds.
  void SilenceAlarms();

  /// Sends a request to the Morse encoder to send `MorseCodeEdge` events for
  /// the current air quality.
  void StartMorseReadout(bool repeat);

  void LogStateChange(const char* old_state) const;

  std::optional<uint16_t> air_quality_;
  pw::InlineString<4> air_quality_score_string_;

  bool alarm_ = false;
  bool alarm_silenced_ = false;
  uint16_t alarm_threshold_ = static_cast<uint16_t>(AirSensor::Score::kYellow);
  HysteresisEdgeDetector<uint16_t> edge_detector_;

  PubSub* pubsub_;
  AmbientLightAdjustedLed led_;

  CommonBaseUnion<State,
                  MonitorMode,
                  ThresholdMode,
                  AlarmMode,
                  MorseReadoutMode>
      state_;
};

}  // namespace sense

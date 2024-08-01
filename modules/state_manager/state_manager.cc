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

#include <cmath>

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_string/format.h"

namespace sense {

template <typename T>
static void AddAndSmoothExponentially(std::optional<T>& aggregate,
                                      T next_value) {
  static constexpr T kDecayFactor = T(4);
  if (!aggregate.has_value()) {
    aggregate = next_value;
  } else {
    *aggregate += (next_value - *aggregate) / kDecayFactor;
  }
}

StateManager::StateManager(PubSub& pubsub, PolychromeLed& led)
    : edge_detector_(alarm_threshold_, alarm_threshold_ + kThresholdIncrement),
      pubsub_(pubsub),
      led_(led),
      state_(*this) {
  pubsub_.Subscribe([this](Event event) { Update(event); });
}

void StateManager::Update(Event event) {
  switch (static_cast<EventType>(event.index())) {
    case kAirQuality:
      UpdateAirQuality(std::get<AirQuality>(event).score);
      break;
    case kButtonA:
      if (std::get<ButtonA>(event).pressed()) {
        state_.get().ButtonAPressed();
      }
      break;
    case kButtonB:
      if (std::get<ButtonB>(event).pressed()) {
        state_.get().ButtonBPressed();
      }
      break;
    case kButtonX:
      if (std::get<ButtonX>(event).pressed()) {
        state_.get().ButtonXPressed();
      }
      break;
    case kButtonY:
      if (std::get<ButtonY>(event).pressed()) {
        state_.get().ButtonYPressed();
      }
      break;
    case kTimerExpired:
      state_.get().OnTimerExpired(std::get<TimerExpired>(event));
      break;
    case kMorseCodeValue:
      state_.get().MorseCodeEdge(std::get<MorseCodeValue>(event));
      break;
    case kAmbientLightSample:
      led_.UpdateBrightnessFromAmbientLight(
          std::get<AmbientLightSample>(event).sample_lux);
      break;
    case kTimerRequest:
    case kMorseEncodeRequest:
    case kProximitySample:
    case kProximityStateChange:
      break;  // ignore these events
  }
}

void StateManager::DisplayThreshold() {
  led_.SetColor(AirSensor::GetLedValue(alarm_threshold_));
  pubsub_.Publish(TimerRequest{
      .token = kThresholdModeToken,
      .timeout_s = kThresholdModeTimeout,
  });
}

void StateManager::IncrementThreshold() {
  if (alarm_threshold_ < kMaxThreshold) {
    SetAlarmThreshold(alarm_threshold_ + kThresholdIncrement);
  }
  DisplayThreshold();
}

void StateManager::DecrementThreshold() {
  if (alarm_threshold_ > 0) {
    SetAlarmThreshold(alarm_threshold_ - kThresholdIncrement);
  }
  DisplayThreshold();
}

void StateManager::SetAlarmThreshold(uint16_t alarm_threshold) {
  alarm_threshold_ = alarm_threshold;
  auto silence_threshold =
      static_cast<uint16_t>(alarm_threshold_ + kThresholdIncrement);
  edge_detector_.set_low_and_high_thresholds(alarm_threshold_,
                                             silence_threshold);
  PW_LOG_INFO("Air quality thresholds set: alarm at %u, silence at %u",
              alarm_threshold_,
              silence_threshold);
  edge_detector_.Update(AirSensor::kMaxScore);
}

void StateManager::UpdateAirQuality(uint16_t score) {
  AddAndSmoothExponentially(air_quality_, score);
  state_.get().OnLedValue(AirSensor::GetLedValue(*air_quality_));
  if (alarm_silenced_) {
    return;
  }
  switch (edge_detector_.Update(*air_quality_)) {
    case Edge::kFalling:
      alarm_ = true;
      break;
    case Edge::kRising:
      alarm_ = false;
      break;
    case Edge::kNone:
      return;
  }
  ResetMode();
}

void StateManager::SilenceAlarms() {
  alarm_ = false;
  alarm_silenced_ = true;
  edge_detector_.Update(AirSensor::kMaxScore);
  pubsub_.Publish(TimerRequest{
      .token = kSilenceAlarmsToken,
      .timeout_s = kSilenceAlarmsTimeout,
  });
  ResetMode();
}

void StateManager::ResetMode() {
  if (alarm_) {
    SetState<AlarmMode>();
  } else {
    SetState<MonitorMode>();
  }
}

void StateManager::StartMorseReadout(bool repeat) {
  if (!air_quality_.has_value()) {
    return;
  }
  pw::Status status = pw::string::FormatOverwrite(
      air_quality_score_string_, "%hu", *air_quality_);
  PW_CHECK_OK(status);
  pubsub_.Publish(MorseEncodeRequest{.message = air_quality_score_string_,
                                      .repeat = repeat ? 0u : 1u});
  PW_LOG_INFO("Current air quality score: %hu", *air_quality_);
}

AmbientLightAdjustedLed::AmbientLightAdjustedLed(PolychromeLed& led)
    : led_(led) {
  led_.SetColor(0);
  led_.SetBrightness(kDefaultBrightness);
  led_.Enable();
  led_.TurnOn();
}

void AmbientLightAdjustedLed::UpdateBrightnessFromAmbientLight(
    float ambient_light_sample_lux) {
  AddAndSmoothExponentially(ambient_light_lux_, ambient_light_sample_lux);

  static constexpr float kMinLux = 40.f;
  static constexpr float kMaxLux = 3000.f;
  if (!ambient_light_lux_.has_value()) {
    return;
  }
  uint8_t brightness;
  if (*ambient_light_lux_ < kMinLux) {
    brightness = kMinBrightness;
  } else if (*ambient_light_lux_ > kMaxLux) {
    brightness = kMaxBrightness;
  } else {
    constexpr float kBrightnessRange = kMaxBrightness - kMinBrightness;
    brightness = static_cast<uint8_t>(
        std::lround((*ambient_light_lux_ - kMinLux) / (kMaxLux - kMinLux) *
                    kBrightnessRange) +
        kMinBrightness);
  }

  PW_LOG_DEBUG("Ambient light: mean_lux=%.1f, brightness=%hhu",
               *ambient_light_lux_,
               brightness);
  led_.SetBrightness(brightness);
}

void StateManager::LogStateChange(const char* old_state) const {
  PW_LOG_INFO("StateManager: %s -> %s", old_state, state_.get().name());
}

}  // namespace sense

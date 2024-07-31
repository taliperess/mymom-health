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

void LedOutputStateMachine::UpdateLed(uint8_t red,
                                      uint8_t green,
                                      uint8_t blue,
                                      uint8_t brightness) {
  if (red_ != red || green_ != green || blue_ != blue) {
    red_ = red;
    green_ = green;
    blue_ = blue;
    if (state_ == kPassthrough) {
      led_->SetColor(red_, green_, blue_);
    }
  }
  if (brightness_ != brightness) {
    brightness_ = brightness;
    if (state_ == kPassthrough) {
      led_->SetBrightness(brightness_);
    }
  }
}

StateManager::StateManager(PubSub& pubsub, PolychromeLed& led)
    : edge_detector_(alarm_threshold_, alarm_threshold_ + kThresholdIncrement),
      pubsub_(&pubsub),
      led_(led, brightness_),
      state_(*this) {
  pubsub_->Subscribe([this](Event event) { Update(event); });
}

void StateManager::Update(Event event) {
  switch (static_cast<EventType>(event.index())) {
    case kAirQuality:
      UpdateAirQuality(std::get<AirQuality>(event).score);
      break;
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
    case kTimerExpired:
      state_.get().OnTimerExpired(std::get<TimerExpired>(event));
      break;
    case kMorseCodeValue:
      state_.get().MorseCodeEdge(std::get<MorseCodeValue>(event));
      break;
    case kAmbientLightSample:
      UpdateAverageAmbientLight(std::get<AmbientLightSample>(event).sample_lux);
      state_.get().AmbientLightUpdate();
      break;
    case kTimerRequest:
    case kMorseEncodeRequest:
    case kProximitySample:
    case kProximityStateChange:
      break;  // ignore these events
  }
}

void StateManager::HandleButtonPress(bool pressed, void (State::* function)()) {
  if (pressed) {
    led_.Override(0xffffff, 160);  // Bright white while pressed.
  } else {
    led_.EndOverride();
    (state_.get().*function)();
  }
}

void StateManager::DisplayThreshold() {
  led_.SetColor(AirSensor::GetLedValue(alarm_threshold_));
  pubsub_->Publish(TimerRequest{
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
  pubsub_->Publish(TimerRequest{
      .token = kSilenceAlarmsToken,
      .timeout_s = kSilenceAlarmsTimeout,
  });
  ResetMode();
}

void StateManager::ResetMode() {
  if (alarm_) {
    SetState<AirQualityAlarmMode>();
  } else {
    SetState<AirQualityMode>();
  }
}

void StateManager::StartMorseReadout(bool repeat) {
  if (!air_quality_.has_value()) {
    return;
  }
  pw::Status status = pw::string::FormatOverwrite(
      air_quality_score_string_, "%hu", *air_quality_);
  PW_CHECK_OK(status);
  pubsub_->Publish(MorseEncodeRequest{.message = air_quality_score_string_,
                                      .repeat = repeat ? 0u : 1u});
  PW_LOG_INFO("Current air quality score: %hu", *air_quality_);
}

void StateManager::UpdateAverageAmbientLight(float ambient_light_sample_lux) {
  AddAndSmoothExponentially(ambient_light_lux_, ambient_light_sample_lux);
}

void StateManager::UpdateBrightnessFromAmbientLight() {
  static constexpr float kMinLux = 40.f;
  static constexpr float kMaxLux = 3000.f;
  if (!ambient_light_lux_.has_value()) {
    return;
  }
  if (*ambient_light_lux_ < kMinLux) {
    brightness_ = kMinBrightness;
  } else if (*ambient_light_lux_ > kMaxLux) {
    brightness_ = kMaxBrightness;
  } else {
    constexpr float kBrightnessRange = kMaxBrightness - kMinBrightness;
    brightness_ = static_cast<uint8_t>(
        std::lround((*ambient_light_lux_ - kMinLux) / (kMaxLux - kMinLux) *
                    kBrightnessRange) +
        kMinBrightness);
  }

  PW_LOG_DEBUG(
      "Ambient light: mean=%.1f, led=%hhu", *ambient_light_lux_, brightness_);

  led_.SetBrightness(brightness_);
}

void StateManager::LogStateChange(const char* old_state) const {
  PW_LOG_INFO("StateManager: %s -> %s", old_state, state_.get().name());
}

}  // namespace sense

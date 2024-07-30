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

void StateManager::Init() {
  pubsub_->Subscribe([this](Event event) { Update(event); });
}

void StateManager::Update(Event event) {
  switch (static_cast<EventType>(event.index())) {
    case kAirQuality:
      last_air_quality_score_ = std::get<AirQuality>(event).score;
      break;
    case kAlarmStateChange:
      state_.get().AlarmStateChanged(std::get<AlarmStateChange>(event).alarm);
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
    case kLedValueColorRotationMode:
      state_.get().ColorRotationModeLedValue(
          std::get<LedValueColorRotationMode>(event));
      break;
    case kProximitySample:
      prox_samples_.Update(std::get<ProximitySample>(event).sample);
      state_.get().ProximitySample(prox_samples_.Average());
      break;
    case kLedValueAirQualityMode:
      state_.get().AirQualityModeLedValue(
          std::get<LedValueAirQualityMode>(event));
      break;
    case kDemoModeTimerExpired:
      state_.get().DemoModeTimerExpired();
      break;
    case kMorseCodeValue:
      state_.get().MorseCodeEdge(std::get<MorseCodeValue>(event));
      break;
    case kAmbientLightSample:
      UpdateAverageAmbientLight(std::get<AmbientLightSample>(event).sample_lux);
      state_.get().AmbientLightUpdate();
      break;
    case kAlarmSilenceRequest:
    case kAirQualityThreshold:
    case kMorseEncodeRequest:
    case kProximityStateChange:
      break;  // ignore these events
  }
}

void StateManager::HandleButtonPress(bool pressed, void (State::*function)()) {
  if (pressed) {
    led_.Override(0xffffff, 160);  // Bright white while pressed.
  } else {
    led_.EndOverride();
    (state_.get().*function)();
  }
}

void StateManager::StartMorseReadout(bool repeat) {
  pw::Status status = pw::string::FormatOverwrite(
      air_quality_score_string_, "%hu", last_air_quality_score_);
  PW_CHECK_OK(status);
  pubsub_->Publish(MorseEncodeRequest{.message = air_quality_score_string_,
                                      .repeat = repeat ? 0u : 1u});
  PW_LOG_INFO("Current air quality score: %hu", last_air_quality_score_);
}

void StateManager::DisplayThreshold() {
  led_.SetColor(AirSensor::GetLedValue(current_threshold_));
}

void StateManager::IncrementThreshold(
    pw::chrono::SystemClock::duration timeout) {
  demo_mode_timer_.Cancel();
  uint16_t candidate_threshold = current_threshold_ + kThresholdIncrement;
  current_threshold_ =
      candidate_threshold < kMaxThreshold ? candidate_threshold : kMaxThreshold;
  pubsub_->Publish(AirQualityThreshold{
      .alarm = current_threshold_,
      .silence =
          static_cast<uint16_t>(current_threshold_ + kThresholdIncrement),
  });
  DisplayThreshold();
  demo_mode_timer_.InvokeAfter(timeout);
}

void StateManager::DecrementThreshold(
    pw::chrono::SystemClock::duration timeout) {
  demo_mode_timer_.Cancel();
  if (current_threshold_ > 0) {
    current_threshold_ -= kThresholdIncrement;
  }
  pubsub_->Publish(AirQualityThreshold{
      .alarm = current_threshold_,
      .silence =
          static_cast<uint16_t>(current_threshold_ + kThresholdIncrement),
  });
  DisplayThreshold();
  demo_mode_timer_.InvokeAfter(timeout);
}

void StateManager::UpdateAverageAmbientLight(float ambient_light_sample_lux) {
  static constexpr float kDecayFactor = 0.25;
  if (std::isnan(ambient_light_lux_)) {
    ambient_light_lux_ = ambient_light_sample_lux;
  } else {
    ambient_light_lux_ +=
        (ambient_light_sample_lux - ambient_light_lux_) * kDecayFactor;
  }
}

void StateManager::UpdateBrightnessFromAmbientLight() {
  static constexpr float kMinLux = 40.f;
  static constexpr float kMaxLux = 3000.f;

  if (ambient_light_lux_ < kMinLux) {
    brightness_ = kMinBrightness;
  } else if (ambient_light_lux_ > kMaxLux) {
    brightness_ = kMaxBrightness;
  } else {
    constexpr float kBrightnessRange = kMaxBrightness - kMinBrightness;
    brightness_ = static_cast<uint8_t>(
        std::lround((ambient_light_lux_ - kMinLux) / (kMaxLux - kMinLux) *
                    kBrightnessRange) +
        kMinBrightness);
  }

  PW_LOG_DEBUG(
      "Ambient light: mean=%.1f, led=%hhu", ambient_light_lux_, brightness_);

  led_.SetBrightness(brightness_);
}

void StateManager::LogStateChange(const char* old_state) const {
  PW_LOG_INFO("StateManager: %s -> %s", old_state, state_.get().name());
}

void StateManager::ProximityDemo::ProximitySample(uint16_t value) {
  // Based on manual experimentation, right shift by 7 (instead of 8) to scale
  // the value, and ignore very low readings to avoid flickering from noise.
  uint8_t scaled = static_cast<uint8_t>(std::min((value >> 7), 255));
  if (scaled < 3) {
    scaled = 0;
  }
  PW_LOG_DEBUG("Proximity: avg=%hu, scaled=%hhu", value, scaled);
  manager().led_.SetBrightness(scaled);
}

}  // namespace sense

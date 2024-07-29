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

#include "modules/air_sensor/air_sensor.h"

#include <cmath>
#include <mutex>

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_status/try.h"

namespace sense {

static constexpr float kHumidityFactor = 0.04f;

LedValue AirSensor::GetLedValue(uint16_t score) {
  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0;
  if (score < 0x100) {
    red = 0xff;
    green = score;
  } else if (score < 0x200) {
    red = static_cast<uint8_t>(0xff - (score - 0x100));
    green = 0xff;
  } else if (score < 0x300) {
    green = 0xff;
    blue = score - 0x200;
  } else {
    green = static_cast<uint8_t>(0xff - (score - 0x300));
    blue = 0xff;
  }
  return LedValue(red, green, blue);
}

AirSensor::AirSensor() : edge_detector_(0, 0) {
  SetThresholds(Score::kYellow, Score::kLightGreen);
}

float AirSensor::temperature() const {
  std::lock_guard lock(lock_);
  return temperature_.value();
}

float AirSensor::pressure() const {
  std::lock_guard lock(lock_);
  return pressure_.value();
}

float AirSensor::humidity() const {
  std::lock_guard lock(lock_);
  return humidity_.value();
}

float AirSensor::gas_resistance() const {
  std::lock_guard lock(lock_);
  return gas_resistance_.value();
}

uint16_t AirSensor::score() const {
  std::lock_guard lock(lock_);
  return score_.value();
}

pw::Status AirSensor::Init(PubSub& pubsub,
                           pw::chrono::VirtualSystemClock& clock) {
  pubsub_ = &pubsub;
  clock_ = &clock;
  return DoInit();
}

void AirSensor::SetThresholds(uint16_t alarm, uint16_t silence) {
  std::lock_guard lock(lock_);
  edge_detector_.set_low_and_high_thresholds(alarm, silence);
  PW_LOG_INFO(
      "Air quality thresholds set: alarm at %u, silence at %u", alarm, silence);
  edge_detector_.Update(kMaxScore);
}

void AirSensor::SetThresholds(Score alarm, Score silence) {
  SetThresholds(static_cast<uint16_t>(alarm), static_cast<uint16_t>(silence));
}

void AirSensor::Silence(pw::chrono::SystemClock::duration duration) {
  // Set by `Init`.
  PW_CHECK_NOTNULL(clock_);
  PW_CHECK_NOTNULL(pubsub_);

  {
    std::lock_guard lock(lock_);
    ignore_until_ = clock_->now() + duration;
  }
  pubsub_->Publish(AlarmStateChange{.alarm = false});
}

pw::Result<uint16_t> AirSensor::MeasureSync() {
  pw::sync::ThreadNotification notification;
  PW_TRY(Measure(notification));
  notification.acquire();
  return score();
}

void AirSensor::Update(float temperature,
                       float pressure,
                       float humidity,
                       float gas_resistance) {
  std::lock_guard lock(lock_);
  // If the alarmed was silenced for some duration that has now elapsed, reset
  // the edge detector to the highest value.
  if (ignore_until_.has_value() && clock_ != nullptr &&
      *ignore_until_ <= clock_->now()) {
    ignore_until_.reset();
    edge_detector_.Update(kMaxScore);
  }

  // Record the sensor data.
  temperature_.Set(temperature);
  pressure_.Set(pressure);
  humidity_.Set(humidity);
  gas_resistance_.Set(gas_resistance);

  // Update the aggregate air qualities values.
  count_.Increment();
  float average = average_.value();
  float quality = gas_resistance < 1.f
                      ? 0.f
                      : (std::log(gas_resistance) + kHumidityFactor * humidity);
  float delta = quality - average;
  average += delta / count_.value();
  float sum_of_squares =
      sum_of_squares_.value() + (delta * (quality - average));
  average_.Set(average);
  quality_.Set(quality);
  sum_of_squares_.Set(sum_of_squares);

  // Calculate the air quality score.
  size_t count = count_.value();
  if (count < 2) {
    return;
  }
  float stddev = std::sqrt(sum_of_squares / (count - 1));
  if (stddev == 0.f) {
    score_.Set(kAverageScore);
    return;
  }
  float score = ((quality - average) / stddev) + 3.f;
  score = std::min(std::max(score * 256.f, 0.f), static_cast<float>(kMaxScore));
  score_.Set(static_cast<uint32_t>(score));

  // Check if a threshold was crossed.
  if (pubsub_ != nullptr) {
    switch (edge_detector_.Update(score_.value())) {
      case Edge::kFalling:
        pubsub_->Publish(AlarmStateChange{.alarm = true});
        break;
      case Edge::kRising:
        pubsub_->Publish(AlarmStateChange{.alarm = false});
        break;
      case Edge::kNone:
        break;
    }
  }
}

}  // namespace sense

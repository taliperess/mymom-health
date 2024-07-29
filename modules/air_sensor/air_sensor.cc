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

#include "pw_status/try.h"

namespace sense {

static constexpr float kHumidityFactor = 0.04f;

float AirSensor::CalculateQuality(float humidity, float gas_resistance) {
  return std::log(gas_resistance) + kHumidityFactor * humidity;
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

uint16_t AirSensor::GetScore() const {
  size_t count;
  float quality, average, sum_of_squares;
  {
    std::lock_guard lock(lock_);
    count = count_.value();
    quality = quality_.value();
    average = average_.value();
    sum_of_squares = sum_of_squares_.value();
  }
  if (count < 2) {
    return kAverageScore;
  }
  float stddev = std::sqrt(sum_of_squares / (count - 1));
  float score = ((quality - average) / stddev) + 3.f;
  if (std::isnan(score)) {
    return kAverageScore;
  }
  score = std::min(std::max(score * 256.f, 0.f), 1023.f);
  return static_cast<uint16_t>(score);
}

pw::Result<uint16_t> AirSensor::MeasureSync() {
  pw::sync::ThreadNotification notification;
  PW_TRY(Measure(notification));
  notification.acquire();
  return GetScore();
}

void AirSensor::Update(float temperature,
                       float pressure,
                       float humidity,
                       float gas_resistance) {
  std::lock_guard lock(lock_);
  temperature_.Set(temperature);
  pressure_.Set(pressure);
  humidity_.Set(humidity);
  gas_resistance_.Set(gas_resistance);
  count_.Increment();
  float average = average_.value();
  float quality = AirSensor::CalculateQuality(humidity, gas_resistance);
  float delta = quality - average;
  average += delta / count_.value();
  sum_of_squares_.Set(sum_of_squares_.value() + (delta * (quality - average)));
  average_.Set(average);
  quality_.Set(quality);
}

}  // namespace sense

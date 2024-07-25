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

#define PW_LOG_MODULE_NAME "AIR_SENSOR"
#define PW_LOG_LEVEL PW_LOG_LEVEL_WARN

#include "modules/air_sensor/air_sensor.h"

#include <cmath>
#include <mutex>

#include "pw_log/log.h"
#include "pw_status/try.h"

namespace sense {

static constexpr float kHumidityFactor = 0.04f;

float AirSensor::temperature() const {
  std::lock_guard lock(lock_);
  return temperature_;
}

float AirSensor::pressure() const {
  std::lock_guard lock(lock_);
  return pressure_;
}

float AirSensor::humidity() const {
  std::lock_guard lock(lock_);
  return humidity_;
}

float AirSensor::gas_resistance() const {
  std::lock_guard lock(lock_);
  return gas_resistance_;
}

uint16_t AirSensor::GetScore() const {
  std::lock_guard lock(lock_);
  if (count_ < 2) {
    return 768;
  }
  float stddev = std::sqrt(sum_of_squares_ / (count_ - 1));
  float score = ((quality_ - average_) / stddev) + 3.f;
  score = std::min(std::max(score * 256.f, 0.f), 1023.f);
  PW_LOG_INFO("count=%zu, quality=%f, average=%f, stddev=%f, score=%f",
              count_,
              quality_,
              average_,
              stddev,
              score);
  return static_cast<uint16_t>(score);
}  //

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
  PW_LOG_INFO("temp=%f, press=%f, humid=%f, resist=%f",
              temperature,
              pressure,
              humidity,
              gas_resistance);
  std::lock_guard lock(lock_);
  temperature_ = temperature;
  pressure_ = pressure;
  humidity_ = humidity;
  gas_resistance_ = gas_resistance;
  ++count_;
  quality_ = std::log(gas_resistance_) + kHumidityFactor * humidity_;
  float delta = quality_ - average_;
  average_ += delta / count_;
  sum_of_squares_ += delta * (quality_ - average_);
}

}  // namespace sense

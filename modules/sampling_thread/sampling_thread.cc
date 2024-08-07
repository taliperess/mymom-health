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

#include "modules/sampling_thread/sampling_thread.h"

#include <chrono>

#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"
#include "pw_log/log.h"
#include "pw_thread/sleep.h"
#include "system/pubsub.h"
#include "system/system.h"

namespace sense {
namespace {

using pw::chrono::SystemClock;

constexpr SystemClock::duration kPeriod = std::chrono::milliseconds(250);

void ReadProximity() {
  pw::Result<uint16_t> sample = system::ProximitySensor().ReadSample();
  if (!sample.ok()) {
    PW_LOG_WARN("Failed to read proximity sensor sample: %s",
                sample.status().str());
    return;
  }
  std::ignore = system::PubSub().Publish(ProximitySample{*sample});
}

void ReadAmbientLight() {
  pw::Result<float> sample = system::AmbientLightSensor().ReadSampleLux();
  if (!sample.ok()) {
    PW_LOG_WARN("Failed to read ambient light sensor sample: %s",
                sample.status().str());
    return;
  }
  std::ignore = system::PubSub().Publish(AmbientLightSample{*sample});
}

void ReadAirSensor() {
  auto& air_sensor = system::AirSensor();

  // Read the sensor syncronously to avoid conflicting with other I2C sensors.
  pw::Result<uint16_t> score = air_sensor.MeasureSync();
  if (!score.ok()) {
    PW_LOG_WARN("Failed to read air sensor score: %s", score.status().str());
    return;
  }

  std::ignore = system::PubSub().Publish(AirQuality{*score});
}

[[nodiscard]] bool LogInit(const char* type, pw::Status init_result) {
  if (!init_result.ok()) {
    PW_LOG_WARN("%s sensor init failed: %s", type, init_result.str());
  }
  return init_result.ok();
}

}  // namespace

// Reads sensor samples in a loop and publishes PubSub events for them.
void SamplingLoop() {
  const bool ambient_light_enabled =
      LogInit("Ambient light", system::AmbientLightSensor().Enable());
  const bool prox_enabled =
      LogInit("Proximity", system::ProximitySensor().Enable());
  const bool air_enabled = LogInit("Air", system::AirSensor().Init());

  SystemClock::time_point deadline = SystemClock::now();

  while (true) {
    deadline += kPeriod;
    pw::this_thread::sleep_until(deadline);

    if (ambient_light_enabled) {
      ReadAmbientLight();
    }
    if (prox_enabled) {
      ReadProximity();
    }
    if (air_enabled) {
      ReadAirSensor();
    }
  }
}

}  // namespace sense

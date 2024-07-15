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

#include "modules/air_sensor/service.h"

namespace am {

void AirSensorService::Init(AirSensor& air_sensor) {
  air_sensor_ = &air_sensor;
}

pw::Status AirSensorService::Init2(const pw_protobuf_Empty&, pw_protobuf_Empty&) {
  return air_sensor_->Init();
}

pw::Status AirSensorService::Measure(const pw_protobuf_Empty&, air_sensor_Measurement& response) {
  PW_TRY(air_sensor_->Measure(notification_));
  notification_.acquire();
  response.temperature = air_sensor_->temperature();
  response.pressure = air_sensor_->pressure();
  response.humidity = air_sensor_->humidity();
  response.gas_resistance = air_sensor_->gas_resistance();
  return pw::OkStatus();
}

}  // namespace am

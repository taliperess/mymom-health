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

#include "pw_log/log.h"

namespace am {

void AirSensorService::Init(Worker& worker, AirSensor& air_sensor) {
  worker_ = &worker;
  air_sensor_ = &air_sensor;
}

pw::Status AirSensorService::Measure(const pw_protobuf_Empty&,
                                     air_sensor_Measurement& response) {
  PW_TRY(air_sensor_->Measure(notification_));
  notification_.acquire();
  response.temperature = air_sensor_->temperature();
  response.pressure = air_sensor_->pressure();
  response.humidity = air_sensor_->humidity();
  response.gas_resistance = air_sensor_->gas_resistance();
  return pw::OkStatus();
}

void AirSensorService::MeasureStream(
    const air_sensor_MeasureStreamRequest& request,
    ServerWriter<air_sensor_Measurement>& writer) {
  if (request.sample_interval_ms < 500) {
    writer.Finish(pw::Status::InvalidArgument());
    return;
  }

  sample_interval_ = pw::chrono::SystemClock::for_at_least(
      std::chrono::milliseconds(request.sample_interval_ms));
  sample_writer_ = std::move(writer);

  ScheduleSample();
}

void AirSensorService::SampleCallback(pw::chrono::SystemClock::time_point) {
  float temperature = air_sensor_->temperature();
  float pressure = air_sensor_->pressure();
  float humidity = air_sensor_->humidity();
  float gas_resistance = air_sensor_->gas_resistance();

  pw::Status status = sample_writer_.Write({
      .temperature = temperature,
      .pressure = pressure,
      .humidity = humidity,
      .gas_resistance = gas_resistance,
  });
  if (status.ok()) {
    ScheduleSample();
  } else {
    PW_LOG_INFO("Air Sensor stream closed; ending periodic sampling");
  }
}

void AirSensorService::ScheduleSample() {
  worker_->RunOnce([this]() { sample_timer_.InvokeAfter(sample_interval_); });
}

}  // namespace am

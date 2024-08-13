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

#include "modules/air_sensor/air_sensor.h"
#include "modules/air_sensor/air_sensor.rpc.pb.h"
#include "modules/worker/worker.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono/system_timer.h"
#include "pw_function/function.h"
#include "pw_status/status.h"
#include "pw_sync/thread_notification.h"

namespace sense {

class AirSensorService final
    : public ::air_sensor::pw_rpc::nanopb::AirSensor::Service<
          AirSensorService> {
 public:
  AirSensorService()
      : sample_timer_(
            pw::bind_member<&AirSensorService::SampleCallback>(this)) {}

  void Init(Worker& worker, AirSensor& air_sensor);

  pw::Status Measure(const pw_protobuf_Empty&,
                     air_sensor_Measurement& response);

  void MeasureStream(const air_sensor_MeasureStreamRequest& request,
                     ServerWriter<air_sensor_Measurement>& writer);

  pw::Status LogMetrics(const pw_protobuf_Empty&, pw_protobuf_Empty&);

 private:
  void SampleCallback(pw::chrono::SystemClock::time_point);

  void ScheduleSample();

  void FillMeasurement(air_sensor_Measurement& response);

  Worker* worker_ = nullptr;
  AirSensor* air_sensor_ = nullptr;
  pw::sync::ThreadNotification notification_;
  pw::chrono::SystemTimer sample_timer_;
  pw::chrono::SystemClock::duration sample_interval_;
  ServerWriter<air_sensor_Measurement> sample_writer_;
};

}  // namespace sense

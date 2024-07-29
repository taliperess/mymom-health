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

#include "bme68x_defs.h"
#include "modules/air_sensor/air_sensor.h"
#include "modules/worker/worker.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono/system_timer.h"
#include "pw_i2c/initiator.h"
#include "pw_i2c/register_device.h"
#include "pw_status/status.h"

namespace sense {

class Bme688 : public AirSensor {
 public:
  explicit Bme688(pw::i2c::Initiator& initiator, Worker& worker);

 private:
  pw::Status DoInit() override;

  pw::Status DoMeasure(pw::sync::ThreadNotification& notification) override;

  void GetDataCallback(pw::chrono::SystemClock::time_point);

  pw::Status Check(int8_t result);

  bme68x_dev bme688_;
  bme68x_conf config_;
  bme68x_heatr_conf heater_;
  Worker& worker_;
  pw::i2c::RegisterDevice i2c_device_;
  pw::chrono::SystemTimer get_data_;
  pw::sync::InterruptSpinLock lock_;
  pw::sync::ThreadNotification* notification_ PW_GUARDED_BY(lock_) = nullptr;
};

}  // namespace sense

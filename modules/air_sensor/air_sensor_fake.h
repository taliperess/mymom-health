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
#include "pw_assert/assert.h"
#include "pw_status/status.h"
#include "pw_sync/thread_notification.h"

namespace sense {

class AirSensorFake : public AirSensor {
 public:
  AirSensorFake() = default;

  void set_autopublish(bool autopublish) { autopublish_ = autopublish; }
  void set_temperature(float temperature) { temperature_ = temperature; }
  void set_pressure(float pressure) { pressure_ = pressure; }
  void set_humidity(float humidity) { humidity_ = humidity; }
  void set_gas_resistance(float gas_resistance) {
    gas_resistance_ = gas_resistance;
  }

  void Publish() {
    Update(temperature_, pressure_, humidity_, gas_resistance_);
    {
      std::lock_guard lock(lock_);
      PW_ASSERT(notification_ != nullptr);
      notification_->release();
      notification_ = nullptr;
    }
  }

 private:
  pw::Status DoMeasure(pw::sync::ThreadNotification& notification) override {
    {
      std::lock_guard lock(lock_);
      notification_ = &notification;
    }
    if (autopublish_) {
      Publish();
    } else {
    }
    return pw::OkStatus();
  }

  bool autopublish_ = true;
  float temperature_;
  float pressure_;
  float humidity_;
  float gas_resistance_;
  pw::sync::InterruptSpinLock lock_;
  pw::sync::ThreadNotification* notification_ PW_GUARDED_BY(lock_) = nullptr;
};

}  // namespace sense

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

#include "pw_status/status.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/thread_notification.h"

namespace sense {

class AirSensor {
 public:
  virtual ~AirSensor() = default;

  float temperature() const PW_LOCKS_EXCLUDED(lock_);
  float pressure() const PW_LOCKS_EXCLUDED(lock_);
  float humidity() const PW_LOCKS_EXCLUDED(lock_);
  float gas_resistance() const PW_LOCKS_EXCLUDED(lock_);

  virtual pw::Status Init();
  pw::Status Measure(pw::sync::ThreadNotification& notification) { return DoMeasure(notification); }

 protected:
  AirSensor() = default;

  void Update(float temperature, float pressure, float humidity, float gas_resistance) PW_LOCKS_EXCLUDED(lock_);

 private:
  virtual pw::Status DoMeasure(pw::sync::ThreadNotification& notification) PW_LOCKS_EXCLUDED(lock_) = 0;

  mutable pw::sync::InterruptSpinLock lock_;
  float temperature_ PW_GUARDED_BY(lock_) = 21.f;
  float pressure_ PW_GUARDED_BY(lock_) = 100.f;
  float humidity_ PW_GUARDED_BY(lock_) = 40.f;
  float gas_resistance_ PW_GUARDED_BY(lock_) = 10000.f;
};

}  // namespace sense

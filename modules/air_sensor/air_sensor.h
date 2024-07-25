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

#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/thread_notification.h"

namespace sense {

class AirSensor {
 public:
  virtual ~AirSensor() = default;

  /// Returns the most recent temperature reading.
  float temperature() const PW_LOCKS_EXCLUDED(lock_);

  /// Returns the most recent barometric pressure reading.
  float pressure() const PW_LOCKS_EXCLUDED(lock_);

  /// Returns the most recent relative humidity reading.
  float humidity() const PW_LOCKS_EXCLUDED(lock_);

  /// Returns the most recent gas resistance reading.
  float gas_resistance() const PW_LOCKS_EXCLUDED(lock_);

  /// Sets up the sensor. By default, does nothing.
  pw::Status Init() { return DoInit(); }

  /// Returns a 10-bit air quality score from 0 (terrible) to 1023 (excellent).
  uint16_t GetScore() const PW_LOCKS_EXCLUDED(lock_);

  /// Requests an air measurement.
  ///
  /// When the measurement is complete, ``Update`` will be called and the
  /// given notification will be released.
  pw::Status Measure(pw::sync::ThreadNotification& notification)
      PW_LOCKS_EXCLUDED(lock_) {
    return DoMeasure(notification);
  }

  /// Like `Measure`, but runs synchronously and returns the same score as
  /// `GetScore`.
  pw::Result<uint16_t> MeasureSync() PW_LOCKS_EXCLUDED(lock_);

 protected:
  AirSensor() = default;

  /// Records the results of an air measurement.
  void Update(float temperature,
              float pressure,
              float humidity,
              float gas_resistance) PW_LOCKS_EXCLUDED(lock_);

 private:
  /// @copydoc `AirSensor::Init`.
  virtual pw::Status DoInit() { return pw::OkStatus(); }

  /// @copydoc `AirSensor::Measure`.
  virtual pw::Status DoMeasure(pw::sync::ThreadNotification& notification)
      PW_LOCKS_EXCLUDED(lock_) = 0;

  mutable pw::sync::InterruptSpinLock lock_;

  // Directly read values.
  float temperature_ PW_GUARDED_BY(lock_) = 21.f;
  float pressure_ PW_GUARDED_BY(lock_) = 100.f;
  float humidity_ PW_GUARDED_BY(lock_) = 40.f;
  float gas_resistance_ PW_GUARDED_BY(lock_) = 10000.f;

  // Derived values.
  size_t count_ PW_GUARDED_BY(lock_) = 0;
  float quality_ PW_GUARDED_BY(lock_) = 0.f;
  float average_ PW_GUARDED_BY(lock_) = 0.f;
  float sum_of_squares_ PW_GUARDED_BY(lock_) = 0.f;
};

}  // namespace sense

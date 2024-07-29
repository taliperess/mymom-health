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

#include "modules/edge_detector/hysteresis_edge_detector.h"
#include "modules/pubsub/pubsub_events.h"
#include "pw_chrono/system_clock.h"
#include "pw_metric/metric.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/thread_notification.h"

namespace sense {

class AirSensor {
 public:
  // Default starting values representing decent air quality.
  static constexpr float kDefaultTemperature = 20.f;
  static constexpr float kDefaultPressure = 100.f;
  static constexpr float kDefaultHumidity = 40.f;
  static constexpr float kDefaultGasResistance = 50000.f;

  /// Threshold presets for convenience.
  ///
  /// The AirSensor is not connected to any output directly, and thus the use of
  /// colors as names for the various thresholds is strictly speaking only to
  /// provide an intuitive idea of the range from very bad (kRed) to very good
  /// (kBlue) air quality. Note that the thresholds for raising and silencing
  /// alarms can be set to any 10 bit values. This enum is strictly for
  /// convenience.
  enum class Score : uint16_t {
    kRed = 0,
    kOrange = 128,
    kYellow = 256,
    kLightGreen = 384,
    kGreen = 512,
    kBlueGreen = 640,
    kCyan = 768,
    kLighBlue = 896,
    kBlue = 1023,
  };

  static constexpr uint16_t kMaxScore = static_cast<uint16_t>(Score::kBlue);
  static constexpr uint16_t kAverageScore = static_cast<uint16_t>(Score::kCyan);
  static constexpr uint16_t kDefaultTheshold =
      static_cast<uint16_t>(Score::kYellow);

  static LedValue GetLedValue(uint16_t score);

  virtual ~AirSensor() = default;

  /// Returns the most recent temperature reading.
  float temperature() const PW_LOCKS_EXCLUDED(lock_);

  /// Returns the most recent barometric pressure reading.
  float pressure() const PW_LOCKS_EXCLUDED(lock_);

  /// Returns the most recent relative humidity reading.
  float humidity() const PW_LOCKS_EXCLUDED(lock_);

  /// Returns the most recent gas resistance reading.
  float gas_resistance() const PW_LOCKS_EXCLUDED(lock_);

  /// Returns a 10-bit air quality score from 0 (terrible) to 1023 (excellent).
  uint16_t score() const PW_LOCKS_EXCLUDED(lock_);

  /// Sets up the sensor.
  pw::Status Init(PubSub& pubsub, pw::chrono::VirtualSystemClock& clock);

  /// Sets the thresholds at which the air sensor will raise or silence an
  /// alarm.
  void SetThresholds(uint16_t alarm, uint16_t silence);
  void SetThresholds(Score alarm, Score silence);

  /// Silences the alarm until the given time has elapsed.
  void Silence(pw::chrono::SystemClock::duration duration);

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

  /// Writes the metrics to logs.
  void LogMetrics() { metrics_.Dump(); }

 protected:
  AirSensor();

  /// Records the results of an air measurement.
  void Update(float temperature,
              float pressure,
              float humidity,
              float gas_resistance) PW_LOCKS_EXCLUDED(lock_);

 private:
  /// @copydoc `AirSensor::Init`.
  ///
  /// By default, does nothing.
  virtual pw::Status DoInit() { return pw::OkStatus(); }

  /// @copydoc `AirSensor::Measure`.
  virtual pw::Status DoMeasure(pw::sync::ThreadNotification& notification)
      PW_LOCKS_EXCLUDED(lock_) = 0;

  mutable pw::sync::InterruptSpinLock lock_;

  // Thread safety: set by `Init`, `const` after that.
  PubSub* pubsub_ = nullptr;
  pw::chrono::VirtualSystemClock* clock_ = nullptr;

  HysteresisEdgeDetector<uint16_t> edge_detector_ PW_GUARDED_BY(lock_);
  std::optional<pw::chrono::SystemClock::time_point> ignore_until_ PW_GUARDED_BY(lock_);

  // Thread safety: metric values should be atomic.
  //
  // Currently, they are not due to a bug, so they are guarded by
  // `lock_`. Unfortunately it isn't possible to use a PW_GUARDED_BY annotation
  // on them.

  PW_METRIC_GROUP(metrics_, "air sensor");

  // // Directly read values.
  PW_METRIC(metrics_, temperature_, "ambient temperature", kDefaultTemperature);
  PW_METRIC(metrics_, pressure_, "barometric pressure", kDefaultPressure);
  PW_METRIC(metrics_, humidity_, "relative humidity", kDefaultHumidity);
  PW_METRIC(metrics_, gas_resistance_, "gas resistance", kDefaultGasResistance);

  // // Derived values.
  PW_METRIC(metrics_, count_, "number of measurements", 0u);
  PW_METRIC(metrics_, quality_, "current air quality", 0.f);
  PW_METRIC(metrics_, average_, "average air quality", 0.f);
  PW_METRIC(metrics_, sum_of_squares_, "aggregate air quality variance", 0.f);
  PW_METRIC(metrics_, score_, "air quality score", kAverageScore);
};

}  // namespace sense

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

#include "modules/air_sensor/air_sensor.h"

#include "modules/air_sensor/air_sensor_fake.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_thread/test_thread_context.h"
#include "pw_thread/thread.h"
#include "pw_unit_test/framework.h"

namespace sense {

// Test fixtures.

class AirSensorTest : public ::testing::Test {
 protected:
  using Score = AirSensor::Score;

  void SetUp() override { air_sensor_.Init(); }

  void MeasureRepeated(size_t n) {
    for (size_t i = 0; i < n; ++i) {
      air_sensor_.MeasureSync().IgnoreError();
    }
  }

  void MeasureRange() {
    float min_ohms = AirSensor::kDefaultGasResistance * 0.5f;
    float max_ohms = AirSensor::kDefaultGasResistance * 1.5f;
    for (float ohms = min_ohms; ohms < max_ohms; ohms += 100.f) {
      air_sensor_.set_gas_resistance(ohms);
      air_sensor_.MeasureSync().IgnoreError();
    }
  }

  AirSensorFake air_sensor_;
  pw::sync::TimedThreadNotification request_;
  pw::sync::TimedThreadNotification response_;
};

// Unit tests.

TEST_F(AirSensorTest, GetScoreBeforeMeasuring) {
  EXPECT_EQ(air_sensor_.score(), AirSensor::kAverageScore);
}

TEST_F(AirSensorTest, MeasureOnce) {
  pw::Result<uint16_t> score = air_sensor_.MeasureSync();
  ASSERT_EQ(score.status(), pw::OkStatus());
  EXPECT_EQ(*score, AirSensor::kAverageScore);
}

TEST_F(AirSensorTest, MeasureIdenticalValues) {
  pw::Result<uint16_t> score;
  for (size_t i = 0; i < 10; ++i) {
    score = air_sensor_.MeasureSync();
    ASSERT_EQ(score.status(), pw::OkStatus());
    EXPECT_EQ(*score, AirSensor::kAverageScore);
  }
}

TEST_F(AirSensorTest, MeasureDifferentValues) {
  MeasureRange();
  pw::Result<uint16_t> score;

  air_sensor_.set_gas_resistance(20000.f);
  score = air_sensor_.MeasureSync();
  ASSERT_EQ(score.status(), pw::OkStatus());
  EXPECT_LT(*score, 256);

  air_sensor_.set_gas_resistance(30000.f);
  score = air_sensor_.MeasureSync();
  ASSERT_EQ(score.status(), pw::OkStatus());
  EXPECT_GE(*score, 256);
  EXPECT_LT(*score, 512);

  air_sensor_.set_gas_resistance(40000.f);
  score = air_sensor_.MeasureSync();
  ASSERT_EQ(score.status(), pw::OkStatus());
  EXPECT_GE(*score, 512);
  EXPECT_LT(*score, 768);

  air_sensor_.set_gas_resistance(50000.f);
  score = air_sensor_.MeasureSync();
  ASSERT_EQ(score.status(), pw::OkStatus());
  EXPECT_GE(*score, 768);
}

TEST_F(AirSensorTest, MeasureFarValues) {
  MeasureRange();
  pw::Result<uint16_t> score;

  air_sensor_.set_gas_resistance(AirSensor::kDefaultGasResistance / 100);
  score = air_sensor_.MeasureSync();
  ASSERT_EQ(score.status(), pw::OkStatus());
  EXPECT_GE(*score, 0);

  air_sensor_.set_gas_resistance(AirSensor::kDefaultGasResistance * 100);
  score = air_sensor_.MeasureSync();
  ASSERT_EQ(score.status(), pw::OkStatus());
  EXPECT_GE(*score, 1023);
}

TEST_F(AirSensorTest, MeasureAsync) {
  air_sensor_.set_autopublish(false);

  pw::thread::test::TestThreadContext context;
  pw::thread::Thread thread(context.options(), [this]() {
    for (float i = 0.f; i < 3.f; i += 1.f) {
      request_.acquire();
      air_sensor_.set_temperature(AirSensor::kDefaultTemperature + i);
      air_sensor_.set_pressure(AirSensor::kDefaultPressure - i);
      air_sensor_.set_humidity(AirSensor::kDefaultHumidity + i);
      air_sensor_.set_gas_resistance(AirSensor::kDefaultGasResistance -
                                     (i * 100));
      air_sensor_.Publish();
    }
  });

  for (size_t i = 0; i < 3; ++i) {
    air_sensor_.Measure(response_);
    request_.release();
    response_.acquire();
    EXPECT_EQ(air_sensor_.temperature(), AirSensor::kDefaultTemperature + i);
    EXPECT_EQ(air_sensor_.pressure(), AirSensor::kDefaultPressure - i);
    EXPECT_EQ(air_sensor_.humidity(), AirSensor::kDefaultHumidity + i);
    EXPECT_EQ(air_sensor_.gas_resistance(),
              AirSensor::kDefaultGasResistance - (i * 100));
  }

  thread.join();
}

}  // namespace sense

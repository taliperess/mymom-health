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
#include "pw_sync/thread_notification.h"
#include "pw_thread/test_thread_context.h"
#include "pw_thread/thread.h"
#include "pw_unit_test/framework.h"

namespace sense {

// Test fixtures.

// Unit tests.
TEST(AirSensorTest, GetScoreBeforeMeasuring) {
  AirSensorFake air_sensor;
  EXPECT_EQ(air_sensor.GetScore(), AirSensor::kAverageScore);
}

TEST(AirSensorTest, MeasureOnce) {
  AirSensorFake air_sensor;

  air_sensor.set_temperature(25.f);
  air_sensor.set_pressure(80.f);
  air_sensor.set_humidity(50.f);
  air_sensor.set_gas_resistance(50000.f);
  pw::Result<uint16_t> score = air_sensor.MeasureSync();

  ASSERT_TRUE(score.ok());
  EXPECT_EQ(*score, AirSensor::kAverageScore);
}

TEST(AirSensorTest, MeasureIdenticalValues) {
  AirSensorFake air_sensor;

  air_sensor.set_temperature(25.f);
  air_sensor.set_pressure(80.f);
  air_sensor.set_humidity(50.f);
  air_sensor.set_gas_resistance(50000.f);

  pw::Result<uint16_t> score;
  for (size_t i = 0; i < 10; ++i) {
    score = air_sensor.MeasureSync();
    ASSERT_TRUE(score.ok());
  }
  EXPECT_EQ(*score, AirSensor::kAverageScore);
}

TEST(AirSensorTest, MeasureDifferentValues) {
  AirSensorFake air_sensor;
  air_sensor.set_humidity(50.f);
  for (float x = 20000.f; x <= 80000.f; x += 100.f) {
    air_sensor.set_gas_resistance(x);
    air_sensor.MeasureSync().IgnoreError();
  }
  pw::Result<uint16_t> score;

  air_sensor.set_gas_resistance(20000.f);
  score = air_sensor.MeasureSync();
  ASSERT_TRUE(score.ok());
  EXPECT_LT(*score, 256);

  air_sensor.set_gas_resistance(30000.f);
  score = air_sensor.MeasureSync();
  ASSERT_TRUE(score.ok());
  EXPECT_GE(*score, 256);
  EXPECT_LT(*score, 512);

  air_sensor.set_gas_resistance(40000.f);
  score = air_sensor.MeasureSync();
  ASSERT_TRUE(score.ok());
  EXPECT_GE(*score, 512);
  EXPECT_LT(*score, 768);

  air_sensor.set_gas_resistance(50000.f);
  score = air_sensor.MeasureSync();
  ASSERT_TRUE(score.ok());
  EXPECT_GE(*score, 768);
}

TEST(AirSensorTest, MeasureFarValues) {
  AirSensorFake air_sensor;
  air_sensor.set_humidity(50.f);
  for (float x = 20000.f; x <= 80000.f; x += 100.f) {
    air_sensor.set_gas_resistance(x);
    air_sensor.MeasureSync().IgnoreError();
  }
  pw::Result<uint16_t> score;

  air_sensor.set_gas_resistance(500.f);
  score = air_sensor.MeasureSync();
  ASSERT_TRUE(score.ok());
  EXPECT_GE(*score, 0);

  air_sensor.set_gas_resistance(5000000.f);
  score = air_sensor.MeasureSync();
  ASSERT_TRUE(score.ok());
  EXPECT_GE(*score, 1023);
}

struct AirSensorContext {
  AirSensorFake air_sensor;
  pw::sync::ThreadNotification request;
  pw::sync::ThreadNotification response;
};

TEST(AirSensorTest, MeasureAsync) {
  AirSensorContext context;
  context.air_sensor.set_autopublish(false);

  pw::thread::test::TestThreadContext test_thread_context;
  pw::thread::Thread thread(test_thread_context.options(), [&context]() {
    for (float i = 0.f; i < 3.f; i += 1.f) {
      context.request.acquire();
      context.air_sensor.set_temperature(25.f + i);
      context.air_sensor.set_pressure(80.f - i);
      context.air_sensor.set_humidity(50.f + i);
      context.air_sensor.set_gas_resistance(50000.f - (i * 100));
      context.air_sensor.Publish();
    }
  });

  for (size_t i = 0; i < 3; ++i) {
    context.air_sensor.Measure(context.response);
    context.request.release();
    context.response.acquire();
    EXPECT_EQ(context.air_sensor.temperature(), 25.f + i);
    EXPECT_EQ(context.air_sensor.pressure(), 80.f - i);
    EXPECT_EQ(context.air_sensor.humidity(), 50.f + i);
    EXPECT_EQ(context.air_sensor.gas_resistance(), 50000.f - (i * 100));
  }

  thread.join();
}

}  // namespace sense
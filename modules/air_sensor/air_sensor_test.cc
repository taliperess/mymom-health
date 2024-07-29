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
#include "modules/pubsub/pubsub.h"
#include "modules/pubsub/pubsub_events.h"
#include "modules/worker/test_worker.h"
#include "pw_chrono/simulated_system_clock.h"
#include "pw_status/try.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_thread/test_thread_context.h"
#include "pw_thread/thread.h"
#include "pw_unit_test/framework.h"

namespace sense {

// Test fixtures.

class AirSensorTest : public ::testing::Test {
 public:
  AirSensorTest()
      : ::testing::Test(),
        pubsub_(worker_, event_queue_, subscribers_buffer_) {}

 protected:
  using Score = AirSensor::Score;

  void SetUp() override {
    air_sensor_.Init(pubsub_, clock_);
    air_sensor_.SetThresholds(Score::kYellow, Score::kLightGreen);
    pubsub_.SubscribeTo<AlarmStateChange>([this](AlarmStateChange event) {
      {
        std::lock_guard lock(alarm_lock_);
        alarm_state_ = event.alarm;
      }
      response_.release();
    });
  }

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

  pw::Status TriggerAlarm() {
    air_sensor_.set_gas_resistance(AirSensor::kDefaultGasResistance / 10);
    uint16_t score;
    PW_TRY_ASSIGN(score, air_sensor_.MeasureSync());
    auto threshold = static_cast<uint16_t>(Score::kYellow);
    EXPECT_LT(score, threshold);
    return score < threshold ? pw::OkStatus() : pw::Status::OutOfRange();
  }

  void TearDown() override { worker_.Stop(); }

  bool alarm_state() {
    std::lock_guard lock_(alarm_lock_);
    return alarm_state_;
  }

  sense::TestWorker<> worker_;
  pw::InlineDeque<Event, 4> event_queue_;
  std::array<PubSub::Subscriber, 4> subscribers_buffer_;
  PubSub pubsub_;
  pw::chrono::SimulatedSystemClock clock_;

  AirSensorFake air_sensor_;
  pw::sync::TimedThreadNotification request_;
  pw::sync::TimedThreadNotification response_;

  pw::sync::InterruptSpinLock alarm_lock_;
  bool alarm_state_ PW_GUARDED_BY(alarm_lock_) = false;
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

TEST_F(AirSensorTest, TriggerAlarm) {
  MeasureRepeated(1000);
  ASSERT_EQ(TriggerAlarm(), pw::OkStatus());
  response_.acquire();
  EXPECT_TRUE(alarm_state());
}

TEST_F(AirSensorTest, RecoverFromAlarm) {
  MeasureRepeated(1000);
  ASSERT_EQ(TriggerAlarm(), pw::OkStatus());
  response_.acquire();
  EXPECT_TRUE(alarm_state());

  air_sensor_.set_gas_resistance(50000.f);
  pw::Result<uint16_t> score = air_sensor_.MeasureSync();
  ASSERT_EQ(score.status(), pw::OkStatus());
  ASSERT_GT(*score, 640);
  response_.acquire();
  EXPECT_FALSE(alarm_state());
}

TEST_F(AirSensorTest, SilenceAlarm) {
  MeasureRepeated(1000);
  ASSERT_EQ(TriggerAlarm(), pw::OkStatus());
  response_.acquire();
  EXPECT_TRUE(alarm_state());

  air_sensor_.Silence(
      pw::chrono::SystemClock::for_at_least(std::chrono::hours(1)));
  response_.acquire();
  EXPECT_FALSE(alarm_state());
}

TEST_F(AirSensorTest, RetriggerAlarmLater) {
  MeasureRepeated(1000);
  ASSERT_EQ(TriggerAlarm(), pw::OkStatus());
  response_.acquire();
  EXPECT_TRUE(alarm_state());

  auto duration =
      pw::chrono::SystemClock::for_at_least(std::chrono::milliseconds(100));
  air_sensor_.Silence(duration);
  response_.acquire();
  EXPECT_FALSE(alarm_state());

  clock_.AdvanceTime(duration);
  air_sensor_.MeasureSync().IgnoreError();
  response_.acquire();
  EXPECT_TRUE(alarm_state());
}

TEST_F(AirSensorTest, DelayAlarmWhenSilenced) {
  MeasureRepeated(1000);

  auto duration =
      pw::chrono::SystemClock::for_at_least(std::chrono::milliseconds(100));
  air_sensor_.Silence(duration);
  response_.acquire();
  EXPECT_FALSE(alarm_state());

  auto start = clock_.now();
  ASSERT_EQ(TriggerAlarm(), pw::OkStatus());

  clock_.AdvanceTime(duration);
  response_.acquire();
  EXPECT_TRUE(alarm_state());

  EXPECT_GE(clock_.now() - start, duration);
}

TEST_F(AirSensorTest, ChangeThresholdWhileSilenced) {
  MeasureRepeated(1000);
  ASSERT_EQ(TriggerAlarm(), pw::OkStatus());
  response_.acquire();
  EXPECT_TRUE(alarm_state());

  auto duration =
      pw::chrono::SystemClock::for_at_least(std::chrono::milliseconds(100));
  air_sensor_.Silence(duration);
  response_.acquire();
  EXPECT_FALSE(alarm_state());

  uint16_t score = air_sensor_.score();
  air_sensor_.SetThresholds(score / 2,
                            static_cast<uint16_t>(Score::kLightGreen));

  // Alarm should not retrigger.
  EXPECT_FALSE(response_.try_acquire_for(duration));
  EXPECT_FALSE(alarm_state());
}

}  // namespace sense

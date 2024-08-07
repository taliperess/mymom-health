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

#include "modules/state_manager/state_manager.h"

#include <array>

#include "modules/led/polychrome_led_fake.h"
#include "modules/pubsub/pubsub.h"
#include "modules/pubsub/pubsub_events.h"
#include "modules/worker/test_worker.h"
#include "pw_chrono/system_clock.h"
#include "pw_thread/sleep.h"
#include "pw_unit_test/framework.h"

namespace sense {

using namespace std::chrono_literals;

class StateManagerTest : public ::testing::Test {
 protected:
  StateManagerTest()
      : ::testing::Test(),
        pubsub_(worker_),
        state_manager_(pubsub_, led_),
        event_(TimerExpired{.token = 0}) {}

  void SetUp() override {
    reference_led_.SetColor(0);
    reference_led_.SetBrightness(AmbientLightAdjustedLed::kDefaultBrightness);
    reference_led_.Enable();
    reference_led_.TurnOn();
    led_.EnableWaiting();
  }

  void SetExpectedColor(uint16_t air_quality) {
    auto led_value = AirSensor::GetLedValue(air_quality);
    reference_led_.SetColor(led_value.r(), led_value.g(), led_value.b());
  }
  void SetExpectedColor(AirSensor::Score air_quality) {
    SetExpectedColor(static_cast<uint16_t>(air_quality));
  }
  void SetExpectedBrightness(uint8_t brightness) {
    reference_led_.SetBrightness(brightness);
  }
  uint16_t GetExpectedRed() { return reference_led_.red(); }
  uint16_t GetExpectedGreen() { return reference_led_.green(); }
  uint16_t GetExpectedBlue() { return reference_led_.blue(); }

  void TearDown() override { worker_.Stop(); }

  TestWorker<> worker_;
  GenericPubSubBuffer<Event, 20, 10> pubsub_;
  PolychromeLedFake led_;
  StateManager state_manager_;
  Event event_;
  pw::sync::ThreadNotification morse_encode_request_;
  pw::sync::ThreadNotification timer_request_;

 private:
  PolychromeLedFake reference_led_;
};

TEST_F(StateManagerTest, UpdateAirQuality) {
  uint16_t air_quality = 800;
  ASSERT_TRUE(pubsub_.Publish(AirQuality{.score = air_quality}));
  led_.Await();
  SetExpectedColor(air_quality);
  EXPECT_EQ(led_.red(), GetExpectedRed());
  EXPECT_EQ(led_.green(), GetExpectedGreen());
  EXPECT_EQ(led_.blue(), GetExpectedBlue());
}

TEST_F(StateManagerTest, MorseReadout) {
  // The manager needs at least one score to switch to Morce code mode.
  uint16_t air_quality = 800;
  ASSERT_TRUE(pubsub_.Publish(AirQuality{.score = air_quality}));
  led_.Await();

  ASSERT_TRUE(pubsub_.SubscribeTo<MorseEncodeRequest>(
      [this](MorseEncodeRequest) { morse_encode_request_.release(); }));
  ASSERT_TRUE(pubsub_.Publish(ButtonY(true)));
  morse_encode_request_.acquire();

  // Responds to Morse code edges.
  EXPECT_TRUE(led_.is_on());
  ASSERT_TRUE(pubsub_.Publish(
      MorseCodeValue{.turn_on = false, .message_finished = false}));
  led_.Await();
  EXPECT_FALSE(led_.is_on());
}

TEST_F(StateManagerTest, UpdateAirQualityAndTriggerAlarm) {
  ASSERT_TRUE(pubsub_.SubscribeTo<MorseEncodeRequest>(
      [this](MorseEncodeRequest) { morse_encode_request_.release(); }));

  uint16_t air_quality = 100;
  ASSERT_TRUE(pubsub_.Publish(AirQuality{.score = air_quality}));
  led_.Await();

  SetExpectedColor(air_quality);
  EXPECT_EQ(led_.red(), GetExpectedRed());
  EXPECT_EQ(led_.green(), GetExpectedGreen());
  EXPECT_EQ(led_.blue(), GetExpectedBlue());

  // Alarm triggered; responds to Morse code edges.
  morse_encode_request_.acquire();
  ASSERT_TRUE(pubsub_.Publish(
      MorseCodeValue{.turn_on = false, .message_finished = false}));
  led_.Await();
  EXPECT_FALSE(led_.is_on());
}

TEST_F(StateManagerTest, UpdateAirQualityAndDisableAlarm) {
  ASSERT_TRUE(pubsub_.SubscribeTo<MorseEncodeRequest>(
      [this](MorseEncodeRequest) { morse_encode_request_.release(); }));

  // Alarm triggered; responds to Morse code edges.
  uint16_t air_quality = 200;
  ASSERT_TRUE(pubsub_.Publish(AirQuality{.score = air_quality}));
  led_.Await();

  // Alarm triggered; responds to Morse code edges.
  morse_encode_request_.acquire();
  ASSERT_TRUE(pubsub_.Publish(
      MorseCodeValue{.turn_on = false, .message_finished = true}));
  led_.Await();
  EXPECT_FALSE(led_.is_on());

  air_quality = 1000;
  ASSERT_TRUE(pubsub_.Publish(AirQuality{.score = air_quality}));
  led_.Await();

  // The state manager updates the LED before disabling the alarm. There's no
  // other event to synchronize on, so send another update to synchronize on
  // and ensure the state change is complete.
  ASSERT_TRUE(pubsub_.Publish(AirQuality{.score = air_quality}));
  led_.Await();

  // Don't check for a specific color. The air quality score is smoothed
  // exponentially, so only check that the alarm is disabled and the LED is on.
  EXPECT_TRUE(led_.is_on());
}

TEST_F(StateManagerTest, SilenceAlarm) {
  ASSERT_TRUE(pubsub_.SubscribeTo<MorseEncodeRequest>(
      [this](MorseEncodeRequest) { morse_encode_request_.release(); }));

  // Trigger an alarm.
  ASSERT_TRUE(pubsub_.Publish(AirQuality{.score = 100}));
  led_.Await();

  // Alarm triggered; responds to Morse code edges.
  morse_encode_request_.acquire();
  EXPECT_TRUE(led_.is_on());
  ASSERT_TRUE(pubsub_.Publish(
      MorseCodeValue{.turn_on = false, .message_finished = false}));
  led_.Await();
  EXPECT_FALSE(led_.is_on());

  // Disable alarm.
  ASSERT_TRUE(pubsub_.Publish(ButtonX(true)));

  ASSERT_TRUE(pubsub_.Publish(AirQuality{.score = 100}));
  led_.Await();

  // Alarm disabled; does not respond to Morse code events
  EXPECT_TRUE(led_.is_on());
  ASSERT_TRUE(pubsub_.Publish(
      MorseCodeValue{.turn_on = false, .message_finished = false}));
  EXPECT_FALSE(led_.TryAwaitFor(100ms));
  EXPECT_TRUE(led_.is_on());
}

TEST_F(StateManagerTest, IncrementThresholdAndTimeout) {
  ASSERT_TRUE(pubsub_.SubscribeTo<TimerRequest>([this](TimerRequest request) {
    event_ = request;
    timer_request_.release();
  }));
  ASSERT_TRUE(pubsub_.SubscribeTo<MorseEncodeRequest>(
      [this](MorseEncodeRequest) { morse_encode_request_.release(); }));

  ASSERT_TRUE(pubsub_.Publish(ButtonB(true)));
  led_.Await();
  SetExpectedColor(AirSensor::Score::kYellow);
  EXPECT_EQ(led_.red(), GetExpectedRed());
  EXPECT_EQ(led_.green(), GetExpectedGreen());
  EXPECT_EQ(led_.blue(), GetExpectedBlue());

  timer_request_.acquire();
  TimerRequest request = std::get<TimerRequest>(event_);
  EXPECT_EQ(request.token, StateManager::kThresholdModeToken);
  EXPECT_EQ(request.timeout_s, StateManager::kThresholdModeTimeout);

  ASSERT_TRUE(pubsub_.Publish(ButtonA(true)));
  led_.Await();
  SetExpectedColor(AirSensor::Score::kLightGreen);
  EXPECT_EQ(led_.red(), GetExpectedRed());
  EXPECT_EQ(led_.green(), GetExpectedGreen());
  EXPECT_EQ(led_.blue(), GetExpectedBlue());
  timer_request_.acquire();
  request = std::get<TimerRequest>(event_);
  EXPECT_EQ(request.token, StateManager::kThresholdModeToken);
  EXPECT_EQ(request.timeout_s, StateManager::kThresholdModeTimeout);

  ASSERT_TRUE(pubsub_.Publish(ButtonA(true)));
  led_.Await();
  SetExpectedColor(AirSensor::Score::kGreen);
  EXPECT_EQ(led_.red(), GetExpectedRed());
  EXPECT_EQ(led_.green(), GetExpectedGreen());
  EXPECT_EQ(led_.blue(), GetExpectedBlue());
  timer_request_.acquire();
  request = std::get<TimerRequest>(event_);
  EXPECT_EQ(request.token, StateManager::kThresholdModeToken);
  EXPECT_EQ(request.timeout_s, StateManager::kThresholdModeTimeout);

  ASSERT_TRUE(pubsub_.Publish(ButtonA(true)));
  led_.Await();
  SetExpectedColor(AirSensor::Score::kBlueGreen);
  EXPECT_EQ(led_.red(), GetExpectedRed());
  EXPECT_EQ(led_.green(), GetExpectedGreen());
  EXPECT_EQ(led_.blue(), GetExpectedBlue());
  timer_request_.acquire();
  request = std::get<TimerRequest>(event_);
  EXPECT_EQ(request.token, StateManager::kThresholdModeToken);
  EXPECT_EQ(request.timeout_s, StateManager::kThresholdModeTimeout);

  ASSERT_TRUE(pubsub_.Publish(ButtonA(true)));
  led_.Await();
  SetExpectedColor(AirSensor::Score::kCyan);
  EXPECT_EQ(led_.red(), GetExpectedRed());
  EXPECT_EQ(led_.green(), GetExpectedGreen());
  EXPECT_EQ(led_.blue(), GetExpectedBlue());
  timer_request_.acquire();
  request = std::get<TimerRequest>(event_);
  EXPECT_EQ(request.token, StateManager::kThresholdModeToken);
  EXPECT_EQ(request.timeout_s, StateManager::kThresholdModeTimeout);

  // At max threshold; cannot increase further.
  ASSERT_TRUE(pubsub_.Publish(ButtonA(true)));
  SetExpectedColor(AirSensor::Score::kCyan);
  EXPECT_EQ(led_.red(), GetExpectedRed());
  EXPECT_EQ(led_.green(), GetExpectedGreen());
  EXPECT_EQ(led_.blue(), GetExpectedBlue());
  timer_request_.acquire();
  request = std::get<TimerRequest>(event_);
  EXPECT_EQ(request.token, StateManager::kThresholdModeToken);
  EXPECT_EQ(request.timeout_s, StateManager::kThresholdModeTimeout);

  // Now time out of the threshold mode.
  ASSERT_TRUE(pubsub_.Publish(
      TimerExpired{.token = StateManager::kThresholdModeToken}));
  morse_encode_request_.acquire();
}

TEST_F(StateManagerTest, DecrementThresholdAndTimeout) {
  ASSERT_TRUE(pubsub_.SubscribeTo<TimerRequest>([this](TimerRequest request) {
    event_ = request;
    timer_request_.release();
  }));
  ASSERT_TRUE(pubsub_.SubscribeTo<MorseEncodeRequest>(
      [this](MorseEncodeRequest) { morse_encode_request_.release(); }));

  ASSERT_TRUE(pubsub_.Publish(ButtonA(true)));
  led_.Await();
  SetExpectedColor(AirSensor::Score::kYellow);
  EXPECT_EQ(led_.red(), GetExpectedRed());
  EXPECT_EQ(led_.green(), GetExpectedGreen());
  EXPECT_EQ(led_.blue(), GetExpectedBlue());
  timer_request_.acquire();
  TimerRequest request = std::get<TimerRequest>(event_);
  EXPECT_EQ(request.token, StateManager::kThresholdModeToken);
  EXPECT_EQ(request.timeout_s, StateManager::kThresholdModeTimeout);

  ASSERT_TRUE(pubsub_.Publish(ButtonB(true)));
  led_.Await();
  SetExpectedColor(AirSensor::Score::kOrange);
  EXPECT_EQ(led_.red(), GetExpectedRed());
  EXPECT_EQ(led_.green(), GetExpectedGreen());
  EXPECT_EQ(led_.blue(), GetExpectedBlue());
  timer_request_.acquire();
  request = std::get<TimerRequest>(event_);
  EXPECT_EQ(request.token, StateManager::kThresholdModeToken);
  EXPECT_EQ(request.timeout_s, StateManager::kThresholdModeTimeout);

  ASSERT_TRUE(pubsub_.Publish(ButtonB(true)));
  led_.Await();
  SetExpectedColor(AirSensor::Score::kRed);
  EXPECT_EQ(led_.red(), GetExpectedRed());
  EXPECT_EQ(led_.green(), GetExpectedGreen());
  EXPECT_EQ(led_.blue(), GetExpectedBlue());
  timer_request_.acquire();
  request = std::get<TimerRequest>(event_);
  EXPECT_EQ(request.token, StateManager::kThresholdModeToken);
  EXPECT_EQ(request.timeout_s, StateManager::kThresholdModeTimeout);

  // At min threshold; cannot decrease further.
  ASSERT_TRUE(pubsub_.Publish(ButtonB(true)));
  SetExpectedColor(AirSensor::Score::kRed);
  EXPECT_EQ(led_.red(), GetExpectedRed());
  EXPECT_EQ(led_.green(), GetExpectedGreen());
  EXPECT_EQ(led_.blue(), GetExpectedBlue());
  timer_request_.acquire();
  request = std::get<TimerRequest>(event_);
  EXPECT_EQ(request.token, StateManager::kThresholdModeToken);
  EXPECT_EQ(request.timeout_s, StateManager::kThresholdModeTimeout);

  // Now time out of the threshold mode.
  ASSERT_TRUE(pubsub_.Publish(
      TimerExpired{.token = StateManager::kThresholdModeToken}));
  morse_encode_request_.acquire();
}

TEST_F(StateManagerTest, AdjustBrightness) {
  ASSERT_TRUE(pubsub_.Publish(AmbientLightSample{.sample_lux = 2000.f}));
  led_.Await();
}

TEST_F(StateManagerTest, AdjustBrightnessMin) {
  ASSERT_TRUE(pubsub_.Publish(AmbientLightSample{.sample_lux = 20.f}));
  led_.Await();
  SetExpectedBrightness(AmbientLightAdjustedLed::kMinBrightness);
  EXPECT_EQ(led_.red(), GetExpectedRed());
  EXPECT_EQ(led_.green(), GetExpectedGreen());
  EXPECT_EQ(led_.blue(), GetExpectedBlue());
}

TEST_F(StateManagerTest, AdjustBrightnessMax) {
  ASSERT_TRUE(pubsub_.Publish(AmbientLightSample{.sample_lux = 20000.f}));
  led_.Await();
  SetExpectedBrightness(AmbientLightAdjustedLed::kMaxBrightness);
  EXPECT_EQ(led_.red(), GetExpectedRed());
  EXPECT_EQ(led_.green(), GetExpectedGreen());
  EXPECT_EQ(led_.blue(), GetExpectedBlue());
}

}  // namespace sense

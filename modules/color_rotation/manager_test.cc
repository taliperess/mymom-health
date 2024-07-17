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

#include "modules/color_rotation/manager.h"

#include "modules/worker/test_worker.h"
#include "pw_log/log.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_unit_test/framework.h"

using namespace std::literals::chrono_literals;

namespace am {
class testing::ColorRotationManagerTester {
 public:
  void StepManager(ColorRotationManager& manager) { manager.Update(); }
};

// A test harness for writing tests that use pubsub.
class ManagerTest : public ::testing::Test,
                    public testing::ColorRotationManagerTester {
 public:
  using PubSub = am::GenericPubSub<Event>;

 protected:
  void SetUp() override {
    last_event_ = {};
    events_processed_ = 0;
  }

  LedValueColorRotationMode DoStep(ColorRotationManager& manager) {
    StepManager(manager);
    EXPECT_TRUE(notification_.try_acquire_for(200ms));
    EXPECT_TRUE(last_event_.has_value());
    EXPECT_TRUE(
        std::holds_alternative<LedValueColorRotationMode>(last_event_.value()));
    return std::get<LedValueColorRotationMode>(last_event_.value());
  }

  LedValueColorRotationMode DoNSteps(ColorRotationManager& manager,
                                     size_t num_steps) {
    for (size_t i = 0; i < num_steps - 1; ++i) {
      DoStep(manager);
    }
    return DoStep(manager);
  }

  pw::InlineDeque<Event, 4> event_queue_;
  std::array<typename PubSub::Subscriber, 4> subscribers_buffer_;
  std::optional<Event> last_event_;
  int events_processed_ = 0;
  pw::sync::TimedThreadNotification notification_;
};

TEST_F(ManagerTest, StepsAreInterpolatedBetweenAndWrap) {
  am::TestWorker<> worker;
  PubSub pubsub(worker, event_queue_, subscribers_buffer_);
  pubsub.Subscribe([this](Event event) {
    last_event_ = event;
    events_processed_ += 1;
    notification_.release();
  });
  std::array steps{
      ColorRotationManager::Step{
          .r = 0xff, .g = 0x00, .b = 0x00, .num_cycles = 0x40},
      ColorRotationManager::Step{
          .r = 0xff, .g = 0xff, .b = 0x00, .num_cycles = 0x40},
      ColorRotationManager::Step{
          .r = 0x00, .g = 0xff, .b = 0x00, .num_cycles = 0x40},
  };
  ColorRotationManager manager(steps);
  manager.Init(pubsub, worker);

  // First cycle should yield red.
  auto color_0 = DoStep(manager);
  EXPECT_EQ(color_0.r(), 0xff);
  EXPECT_EQ(color_0.g(), 0x00);
  EXPECT_EQ(color_0.b(), 0x00);

  // Halfway through step should be a mix between red and yellow.
  auto color_1 = DoNSteps(manager, 0x20);
  EXPECT_EQ(color_1.r(), 0xff);
  EXPECT_EQ(color_1.g(), 0x7f);
  EXPECT_EQ(color_1.b(), 0x00);

  // Fully through the step the color should be yellow.
  auto color_2 = DoNSteps(manager, 0x20);
  EXPECT_EQ(color_2.r(), 0xff);
  EXPECT_EQ(color_2.g(), 0xff);
  EXPECT_EQ(color_2.b(), 0x00);

  // Halfway through the next step we should have a mix of yellow and green.
  auto color_3 = DoNSteps(manager, 0x20);
  EXPECT_EQ(color_3.r(), 0x7f);
  EXPECT_EQ(color_3.g(), 0xff);
  EXPECT_EQ(color_3.b(), 0x00);

  // Then fully green.
  auto color_4 = DoNSteps(manager, 0x20);
  EXPECT_EQ(color_4.r(), 0x00);
  EXPECT_EQ(color_4.g(), 0xff);
  EXPECT_EQ(color_4.b(), 0x00);

  // At the end of the steps we should find our way back to red
  auto color_5 = DoNSteps(manager, 0x20);
  EXPECT_EQ(color_5.r(), 0x7f);
  EXPECT_EQ(color_5.g(), 0x7f);
  EXPECT_EQ(color_5.b(), 0x00);

  auto color_6 = DoNSteps(manager, 0x20);
  EXPECT_EQ(color_6.r(), 0xff);
  EXPECT_EQ(color_6.g(), 0x00);
  EXPECT_EQ(color_6.b(), 0x00);

  worker.Stop();
}
}  // namespace am

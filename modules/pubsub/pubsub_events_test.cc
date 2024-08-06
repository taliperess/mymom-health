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

#include "modules/pubsub/pubsub_events.h"

#include "modules/pubsub/pubsub.h"
#include "modules/worker/test_worker.h"
#include "pw_sync/thread_notification.h"
#include "pw_unit_test/framework.h"

namespace {

using namespace std::literals::chrono_literals;

// Test fixtures.

class PubSubEventsTest : public ::testing::Test {
 protected:
  static constexpr size_t kMaxEvents = 4;
  static constexpr size_t kMaxSubscribers = 4;
  using PubSub =
      sense::GenericPubSubBuffer<sense::Event, kMaxEvents, kMaxSubscribers>;

  PubSubEventsTest() : ::testing::Test(), pubsub_(worker_) {}

  void TearDown() override { worker_.Stop(); }

  sense::TestWorker<> worker_;
  PubSub pubsub_;
  uint16_t total_score_ = 0;
  size_t events_processed_ = 0;
  pw::sync::ThreadNotification notification_;
};

// Unit tests.

TEST_F(PubSubEventsTest, PublishEvent) {
  ASSERT_TRUE(pubsub_.Subscribe([this](sense::Event event) {
    if (std::holds_alternative<sense::AirQuality>(event)) {
      total_score_ += std::get<sense::AirQuality>(event).score;
    } else if (std::holds_alternative<sense::ButtonA>(event)) {
      EXPECT_TRUE(std::get<sense::ButtonA>(event).pressed());
    } else {
      FAIL() << "Unexpected event type";
    }
    ++events_processed_;
    if (events_processed_ >= 4) {
      notification_.release();
    }
  }));

  // Block the work queue until all events are published.
  pw::sync::ThreadNotification pause;
  worker_.RunOnce([&pause]() { pause.acquire(); });
  ASSERT_TRUE(pubsub_.Publish(sense::AirQuality{.score = 128u}));
  ASSERT_TRUE(pubsub_.Publish(sense::AirQuality{.score = 256u}));
  ASSERT_TRUE(pubsub_.Publish(sense::ButtonA(true)));
  ASSERT_TRUE(pubsub_.Publish(sense::AirQuality{.score = 384u}));
  pause.release();

  // This should time out as the fifth event never gets sent.
  notification_.acquire();
  EXPECT_EQ(events_processed_, 4u);
  EXPECT_EQ(total_score_, 768u);
}

TEST_F(PubSubEventsTest, SubscribeTo) {
  ASSERT_TRUE(
      pubsub_.SubscribeTo<sense::AirQuality>([this](sense::AirQuality sample) {
        total_score_ += sample.score;
        ++events_processed_;
        if (events_processed_ >= 2) {
          notification_.release();
        }
      }));

  // Block the work queue until all events are published.
  pw::sync::ThreadNotification pause;
  worker_.RunOnce([&pause]() { pause.acquire(); });
  ASSERT_TRUE(pubsub_.Publish(sense::ButtonA(true)));
  ASSERT_TRUE(pubsub_.Publish(sense::AirQuality{.score = 768u}));
  ASSERT_TRUE(pubsub_.Publish(sense::ButtonA(true)));
  ASSERT_TRUE(pubsub_.Publish(sense::AirQuality{.score = 256u}));
  pause.release();

  // This should time out as the fifth event never gets sent.
  notification_.acquire();
  EXPECT_EQ(events_processed_, 2u) << "Only air quality events are processed";
  EXPECT_EQ(total_score_, 1024u);
}

}  // namespace

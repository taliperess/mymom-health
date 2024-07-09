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

#include "modules/pubsub/pubsub.h"

#include "modules/testing/work_queue.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_unit_test/framework.h"

namespace {

using namespace std::literals::chrono_literals;

struct TestEvent {
  int value;
};
using PubSub = am::internal::PubSubBase<TestEvent>;

class PubSubTest : public am::TestWithWorkQueue<> {
 public:
  PubSubTest() {}

  void TearDown() { StopWorkQueue(); }

 protected:
  std::array<PubSub::SubscribeCallback, 4> subscribers_buffer_;
  int result_ = 0;
  pw::sync::TimedThreadNotification notification_;
};

TEST_F(PubSubTest, Publish_OneSubscriber) {
  PubSub pubsub(work_queue(), subscribers_buffer_);

  pubsub.Subscribe([this](TestEvent event) {
    result_ = event.value;
    notification_.release();
  });

  pubsub.Publish({.value = 42});

  notification_.try_acquire_for(500ms);
  EXPECT_EQ(result_, 42);
}

TEST_F(PubSubTest, Publish_MultipleSubscribers) {
  PubSub pubsub(work_queue(), subscribers_buffer_);

  for (size_t i = 0; i < subscribers_buffer_.size(); ++i) {
    struct {
      int* value;
      pw::sync::TimedThreadNotification* notification;
    } context = {
        .value = &result_,
        .notification =
            i == subscribers_buffer_.size() - 1 ? &notification_ : nullptr,
    };

    pubsub.Subscribe([&context](TestEvent event) {
      *context.value += event.value;
      if (context.notification != nullptr) {
        context.notification->release();
      }
    });
  }

  pubsub.Publish({.value = 4});

  notification_.try_acquire_for(500ms);
  EXPECT_EQ(result_, static_cast<int>(4 * subscribers_buffer_.size()));
}

TEST_F(PubSubTest, Subscribe_Full) {
  PubSub pubsub(work_queue(), subscribers_buffer_);

  EXPECT_TRUE(pubsub.Subscribe([this](TestEvent) { notification_.release(); })
                  .has_value());
  EXPECT_EQ(pubsub.subscriber_count(), 1u);
  EXPECT_TRUE(pubsub.Subscribe([this](TestEvent) { notification_.release(); })
                  .has_value());
  EXPECT_EQ(pubsub.subscriber_count(), 2u);
  EXPECT_TRUE(pubsub.Subscribe([this](TestEvent) { notification_.release(); })
                  .has_value());
  EXPECT_EQ(pubsub.subscriber_count(), 3u);
  EXPECT_TRUE(pubsub.Subscribe([this](TestEvent) { notification_.release(); })
                  .has_value());
  EXPECT_EQ(pubsub.subscriber_count(), 4u);
  EXPECT_EQ(pubsub.subscriber_count(), pubsub.max_subscribers());
  EXPECT_FALSE(pubsub.Subscribe([this](TestEvent) { notification_.release(); })
                   .has_value());
  EXPECT_EQ(pubsub.subscriber_count(), 4u);
}

TEST_F(PubSubTest, Subscribe_Unsubscribe) {
  PubSub pubsub(work_queue(), subscribers_buffer_);

  auto token1 =
      pubsub.Subscribe([this](TestEvent) { notification_.release(); });
  EXPECT_EQ(pubsub.subscriber_count(), 1u);
  auto token2 =
      pubsub.Subscribe([this](TestEvent) { notification_.release(); });
  EXPECT_EQ(pubsub.subscriber_count(), 2u);
  auto token3 =
      pubsub.Subscribe([this](TestEvent) { notification_.release(); });
  EXPECT_EQ(pubsub.subscriber_count(), 3u);
  auto token4 =
      pubsub.Subscribe([this](TestEvent) { notification_.release(); });
  EXPECT_EQ(pubsub.subscriber_count(), 4u);

  EXPECT_FALSE(pubsub.Subscribe([this](TestEvent) { notification_.release(); })
                   .has_value());
  EXPECT_EQ(pubsub.subscriber_count(), 4u);

  EXPECT_TRUE(pubsub.Unsubscribe(*token2));
  EXPECT_EQ(pubsub.subscriber_count(), 3u);

  EXPECT_TRUE(pubsub.Subscribe([this](TestEvent) { notification_.release(); })
                  .has_value());
  EXPECT_EQ(pubsub.subscriber_count(), 4u);

  EXPECT_TRUE(pubsub.Unsubscribe(*token1));
  EXPECT_TRUE(pubsub.Unsubscribe(*token3));
  EXPECT_TRUE(pubsub.Unsubscribe(*token4));
  EXPECT_EQ(pubsub.subscriber_count(), 1u);
}

}  // namespace

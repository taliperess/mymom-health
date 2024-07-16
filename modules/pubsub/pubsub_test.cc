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

#include "modules/pubsub/pubsub_events.h"
#include "modules/worker/test_worker.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_unit_test/framework.h"

namespace {

using namespace std::literals::chrono_literals;

struct TestEvent {
  int value;
};
using PubSub = am::GenericPubSub<TestEvent>;

class PubSubTest : public ::testing::Test {
 protected:
  pw::InlineDeque<TestEvent, 4> event_queue_;
  std::array<PubSub::Subscriber, 4> subscribers_buffer_;
  int result_ = 0;
  int events_processed_ = 0;
  pw::sync::TimedThreadNotification notification_;
  pw::sync::TimedThreadNotification work_queue_start_notification_;
};

TEST_F(PubSubTest, Publish_OneSubscriber) {
  am::TestWorker<> worker;
  PubSub pubsub(worker, event_queue_, subscribers_buffer_);

  pubsub.Subscribe([this](TestEvent event) {
    result_ = event.value;
    notification_.release();
  });

  EXPECT_TRUE(pubsub.Publish({.value = 42}));

  // TODO: pwbug.dev/352133474 - Eliminate race conditions from tests.
  EXPECT_TRUE(notification_.try_acquire_for(200ms));
  EXPECT_EQ(result_, 42);
  worker.Stop();
}

TEST_F(PubSubTest, Publish_MultipleSubscribers) {
  am::TestWorker<> worker;
  PubSub pubsub(worker, event_queue_, subscribers_buffer_);

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

  EXPECT_TRUE(pubsub.Publish({.value = 4}));

  EXPECT_TRUE(notification_.try_acquire_for(200ms));
  EXPECT_EQ(result_, static_cast<int>(4 * subscribers_buffer_.size()));
  worker.Stop();
}

TEST_F(PubSubTest, Publish_MultipleEvents) {
  am::TestWorker<> worker;
  PubSub pubsub(worker, event_queue_, subscribers_buffer_);

  pubsub.Subscribe([this](TestEvent event) {
    result_ += event.value;
    events_processed_++;

    if (events_processed_ % 4 == 0) {
      notification_.release();
    }
  });

  EXPECT_TRUE(pubsub.Publish({.value = 1}));
  EXPECT_TRUE(pubsub.Publish({.value = 2}));
  EXPECT_TRUE(pubsub.Publish({.value = 3}));
  EXPECT_TRUE(pubsub.Publish({.value = 4}));

  EXPECT_TRUE(notification_.try_acquire_for(200ms));
  EXPECT_EQ(result_, 10);
  EXPECT_EQ(events_processed_, 4);

  EXPECT_TRUE(pubsub.Publish({.value = 5}));
  EXPECT_TRUE(pubsub.Publish({.value = 6}));
  EXPECT_TRUE(pubsub.Publish({.value = 7}));
  EXPECT_TRUE(pubsub.Publish({.value = 8}));

  EXPECT_TRUE(notification_.try_acquire_for(200ms));
  EXPECT_EQ(result_, 36);
  EXPECT_EQ(events_processed_, 8);
  worker.Stop();
}

TEST_F(PubSubTest, Publish_MultipleEvents_QueueFull) {
  am::TestWorker<> worker;
  PubSub pubsub(worker, event_queue_, subscribers_buffer_);

  worker.RunOnce([this]() {
    // Block the work queue until all events are published.
    PW_ASSERT(work_queue_start_notification_.try_acquire_for(1s));
  });

  pubsub.Subscribe([this](TestEvent event) {
    result_ += event.value;
    events_processed_++;

    if (events_processed_ == 5) {
      notification_.release();
    }
  });

  EXPECT_TRUE(pubsub.Publish({.value = 10}));
  EXPECT_TRUE(pubsub.Publish({.value = 11}));
  EXPECT_TRUE(pubsub.Publish({.value = 12}));
  EXPECT_TRUE(pubsub.Publish({.value = 13}));
  EXPECT_FALSE(pubsub.Publish({.value = 14}));
  work_queue_start_notification_.release();

  // This should time out as the fifth event never gets sent.
  EXPECT_FALSE(notification_.try_acquire_for(1ms));
  EXPECT_EQ(events_processed_, 4);
  EXPECT_EQ(result_, 46);
  worker.Stop();
}

TEST_F(PubSubTest, Subscribe_Full) {
  am::TestWorker<> worker;
  PubSub pubsub(worker, event_queue_, subscribers_buffer_);

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
  worker.Stop();
}

TEST_F(PubSubTest, Subscribe_Unsubscribe) {
  am::TestWorker<> worker;
  PubSub pubsub(worker, event_queue_, subscribers_buffer_);

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
  worker.Stop();
}

class PubSubAmEventsTest : public ::testing::Test {
 protected:
  pw::InlineDeque<am::Event, 4> event_queue_;
  std::array<am::PubSub::Subscriber, 4> subscribers_buffer_;
  float total_voc_ = 0;
  int events_processed_ = 0;
  pw::sync::TimedThreadNotification notification_;
  pw::sync::TimedThreadNotification work_queue_start_notification_;
};

TEST_F(PubSubAmEventsTest, PublishEvent) {
  am::TestWorker<> worker;
  am::PubSub pubsub(worker, event_queue_, subscribers_buffer_);

  worker.RunOnce([this]() {
    // Block the work queue until all events are published.
    PW_ASSERT(work_queue_start_notification_.try_acquire_for(1s));
  });

  pubsub.Subscribe([this](am::Event event) {
    if (std::holds_alternative<am::VocSample>(event)) {
      total_voc_ += std::get<am::VocSample>(event).voc_level;
    } else if (std::holds_alternative<am::ButtonA>(event)) {
      EXPECT_TRUE(std::get<am::ButtonA>(event).pressed());
    } else {
      FAIL() << "Unexpected event type";
    }

    if (++events_processed_ > 4) {
      notification_.release();
    }
  });

  EXPECT_TRUE(pubsub.Publish(am::VocSample{.voc_level = 0.25f}));
  EXPECT_TRUE(pubsub.Publish(am::VocSample{.voc_level = 0.50f}));
  EXPECT_TRUE(pubsub.Publish(am::ButtonA(true)));
  EXPECT_TRUE(pubsub.Publish(am::VocSample{.voc_level = 0.25}));
  work_queue_start_notification_.release();

  // This should time out as the fifth event never gets sent.
  EXPECT_FALSE(notification_.try_acquire_for(1ms));
  EXPECT_EQ(events_processed_, 4);
  EXPECT_EQ(total_voc_, 1.0f);
  worker.Stop();
}

TEST_F(PubSubAmEventsTest, SubscribeTo) {
  am::TestWorker<> worker;
  am::PubSub pubsub(worker, event_queue_, subscribers_buffer_);

  worker.RunOnce([this]() {
    // Block the work queue until all events are published.
    PW_ASSERT(work_queue_start_notification_.try_acquire_for(1s));
  });

  pubsub.SubscribeTo<am::VocSample>([this](am::VocSample sample) {
    total_voc_ += sample.voc_level;
    if (++events_processed_ > 2) {
      notification_.release();
    }
  });

  EXPECT_TRUE(pubsub.Publish(am::ButtonA(true)));
  EXPECT_TRUE(pubsub.Publish(am::VocSample{.voc_level = 0.75f}));
  EXPECT_TRUE(pubsub.Publish(am::ButtonA(true)));
  EXPECT_TRUE(pubsub.Publish(am::VocSample{.voc_level = 0.25}));
  work_queue_start_notification_.release();

  // This should time out as the fifth event never gets sent.
  EXPECT_FALSE(notification_.try_acquire_for(1ms));
  EXPECT_EQ(events_processed_, 2) << "Only voc events are processed";
  EXPECT_EQ(total_voc_, 1.0f);
  worker.Stop();
}

}  // namespace

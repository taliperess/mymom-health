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

#include <mutex>

#include "modules/worker/test_worker.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_sync/thread_notification.h"
#include "pw_unit_test/framework.h"

namespace {

using namespace std::literals::chrono_literals;

// Test fixtures.

struct EchoRequest {
  uint32_t value;
};

class EchoResponse {
 public:
  void SetNotifyAfter(size_t num_events) PW_LOCKS_EXCLUDED(lock_) {
    std::lock_guard lock(lock_);
    if (num_events < events_seen_) {
      notification_.release();
    }
    events_seen_ = 0;
    notify_after_ = num_events;
  }

  std::optional<uint32_t> TryGetValue() PW_LOCKS_EXCLUDED(lock_) {
    std::optional<uint32_t> result;
    if (notification_.try_acquire()) {
      std::lock_guard lock(lock_);
      result = value_;
    }
    return result;
  }

  uint32_t BlockAndGetValue() PW_LOCKS_EXCLUDED(lock_) {
    notification_.acquire();
    std::lock_guard lock(lock_);
    return value_;
  }

  void AddValueAndUnblock(uint32_t value) PW_LOCKS_EXCLUDED(lock_) {
    std::lock_guard lock(lock_);
    value_ += value;
    ++events_seen_;
    if (events_seen_ == notify_after_) {
      notification_.release();
      events_seen_ = 0;
    }
  }

 private:
  uint32_t value_ PW_GUARDED_BY(lock_) = 0;
  size_t events_seen_ PW_GUARDED_BY(lock_) = 0;
  size_t notify_after_ PW_GUARDED_BY(lock_) = 1;
  pw::sync::Mutex lock_;
  pw::sync::ThreadNotification notification_;
};

class PubSubTest : public ::testing::Test {
 protected:
  static constexpr size_t kMaxEvents = 4;
  static constexpr size_t kMaxSubscribers = 4;
  using PubSub =
      sense::GenericPubSubBuffer<EchoRequest, kMaxEvents, kMaxSubscribers>;

  PubSubTest() : ::testing::Test(), pubsub_(worker_) {}

  void TearDown() override { worker_.Stop(); }

  sense::TestWorker<> worker_;
  PubSub pubsub_;
  std::array<EchoResponse, kMaxSubscribers> responses_;
};

// Unit tests.

TEST_F(PubSubTest, Publish_OneSubscriber) {
  EchoResponse& response = responses_[0];
  ASSERT_TRUE(pubsub_.Subscribe([&response](EchoRequest request) {
    response.AddValueAndUnblock(request.value);
  }));
  EXPECT_TRUE(pubsub_.Publish({.value = 42u}));
  EXPECT_EQ(response.BlockAndGetValue(), 42u);
}

TEST_F(PubSubTest, Publish_MultipleSubscribers) {
  for (auto& response : responses_) {
    ASSERT_TRUE(pubsub_.Subscribe([&response](EchoRequest request) {
      response.AddValueAndUnblock(request.value);
    }));
  }
  ASSERT_TRUE(pubsub_.Publish({.value = 4u}));
  for (auto& response : responses_) {
    EXPECT_EQ(response.BlockAndGetValue(), 4u);
  }
}

TEST_F(PubSubTest, Publish_MultipleEvents) {
  EchoResponse& response = responses_[0];
  ASSERT_TRUE(pubsub_.Subscribe([&response](EchoRequest request) {
    response.AddValueAndUnblock(request.value);
  }));
  response.SetNotifyAfter(4);
  ASSERT_TRUE(pubsub_.Publish({.value = 1}));
  ASSERT_TRUE(pubsub_.Publish({.value = 2}));
  ASSERT_TRUE(pubsub_.Publish({.value = 3}));
  ASSERT_TRUE(pubsub_.Publish({.value = 4}));
  EXPECT_EQ(response.BlockAndGetValue(), 10u);

  ASSERT_TRUE(pubsub_.Publish({.value = 5}));
  ASSERT_TRUE(pubsub_.Publish({.value = 6}));
  ASSERT_TRUE(pubsub_.Publish({.value = 7}));
  ASSERT_TRUE(pubsub_.Publish({.value = 8}));
  EXPECT_EQ(response.BlockAndGetValue(), 36u);
}

TEST_F(PubSubTest, Publish_MultipleEvents_QueueFull) {
  EchoResponse& response = responses_[0];

  // Block the work queue until all events are published.
  pw::sync::ThreadNotification pause;
  worker_.RunOnce([&pause]() { pause.acquire(); });

  ASSERT_TRUE(pubsub_.Subscribe([&response](EchoRequest request) {
    response.AddValueAndUnblock(request.value);
  }));
  response.SetNotifyAfter(5);
  ASSERT_TRUE(pubsub_.Publish({.value = 10}));
  ASSERT_TRUE(pubsub_.Publish({.value = 11}));
  ASSERT_TRUE(pubsub_.Publish({.value = 12}));
  ASSERT_TRUE(pubsub_.Publish({.value = 13}));
  EXPECT_FALSE(pubsub_.Publish({.value = 14}));

  // The fifth event never gets sent.
  pause.release();
  auto result = response.TryGetValue();
  EXPECT_FALSE(result.has_value());

  response.SetNotifyAfter(4);
  EXPECT_EQ(response.BlockAndGetValue(), 46u);
}

TEST_F(PubSubTest, Subscribe_Full) {
  for (auto& response : responses_) {
    ASSERT_TRUE(pubsub_.Subscribe([&response](EchoRequest request) {
      response.AddValueAndUnblock(request.value);
    }));
  }

  // Subscriber is not added when max subscribers has been reached.
  EchoResponse extra;
  EXPECT_FALSE(pubsub_.Subscribe([&extra](EchoRequest request) {
    extra.AddValueAndUnblock(request.value);
  }));
  EXPECT_EQ(pubsub_.subscriber_count(), 4u);

  // The extra subscriber never gets events.
  ASSERT_TRUE(pubsub_.Publish({.value = 4u}));
  auto result = extra.TryGetValue();
  EXPECT_FALSE(result.has_value());
}

TEST_F(PubSubTest, Subscribe_Unsubscribe) {
  std::array<PubSub::SubscribeToken, kMaxSubscribers> tokens;
  auto token = tokens.begin();
  for (auto& response : responses_) {
    ASSERT_NE(token, tokens.end());
    auto result = pubsub_.Subscribe([&response](EchoRequest request) {
      response.AddValueAndUnblock(request.value);
    });
    EXPECT_TRUE(result.has_value());
    *token = *result;
    ++token;
  }
  EXPECT_EQ(pubsub_.subscriber_count(), 4u);

  // Remove a subscriber.
  EXPECT_TRUE(pubsub_.Unsubscribe(tokens[1]));
  EXPECT_EQ(pubsub_.subscriber_count(), 3u);

  // Add a subscriber after removing.
  EchoResponse& response = responses_[2];
  EXPECT_TRUE(pubsub_.Subscribe([&response](EchoRequest request) {
    response.AddValueAndUnblock(request.value);
  }));
  EXPECT_EQ(pubsub_.subscriber_count(), 4u);

  // Remove multiple subscriber.
  EXPECT_TRUE(pubsub_.Unsubscribe(tokens[0]));
  EXPECT_TRUE(pubsub_.Unsubscribe(tokens[2]));
  EXPECT_TRUE(pubsub_.Unsubscribe(tokens[3]));
  EXPECT_EQ(pubsub_.subscriber_count(), 1u);

  // Unsubscribe invalid.
  EXPECT_FALSE(pubsub_.Unsubscribe(tokens[1]));
}

}  // namespace

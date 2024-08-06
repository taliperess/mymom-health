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

#include "modules/pubsub/service.h"

#include "modules/pubsub/pubsub_events.h"
#include "modules/worker/test_worker.h"
#include "pw_rpc/nanopb/test_method_context.h"
#include "pw_rpc/test_helpers.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_unit_test/framework.h"

namespace {

using namespace std::literals::chrono_literals;

class PubSubServiceTest : public ::testing::Test {
 protected:
  static constexpr size_t kMaxEvents = 4;
  static constexpr size_t kMaxSubscribers = 4;
  using PubSub =
      sense::GenericPubSubBuffer<sense::Event, kMaxEvents, kMaxSubscribers>;

  PubSubServiceTest() : ::testing::Test(), pubsub_(worker_) {}

  void TearDown() override { worker_.Stop(); }

  sense::TestWorker<> worker_;
  PubSub pubsub_;

  pw::sync::ThreadNotification notification_;
  size_t events_processed_ = 0;
  uint16_t total_score_ = 0;
  size_t button_presses_ = 0;
};

TEST_F(PubSubServiceTest, Subscribe) {
  PW_NANOPB_TEST_METHOD_CONTEXT(sense::PubSubService, Subscribe) ctx;
  ctx.service().Init(pubsub_);
  ctx.call({});

  pw::rpc::test::WaitForPackets(ctx.output(), 3, [this] {
    EXPECT_TRUE(pubsub_.Publish(sense::AirQuality{.score = 256u}));
    EXPECT_TRUE(pubsub_.Publish(sense::ButtonB(false)));
    EXPECT_TRUE(pubsub_.Publish(sense::ButtonY(true)));
  });

  EXPECT_EQ(ctx.responses().size(), 3u);
  ASSERT_EQ(ctx.responses()[0].which_type, pubsub_Event_air_quality_tag);
  EXPECT_EQ(ctx.responses()[0].type.air_quality, 256u);
  ASSERT_EQ(ctx.responses()[1].which_type, pubsub_Event_button_b_pressed_tag);
  EXPECT_EQ(ctx.responses()[1].type.button_b_pressed, false);
  ASSERT_EQ(ctx.responses()[2].which_type, pubsub_Event_button_y_pressed_tag);
  EXPECT_EQ(ctx.responses()[2].type.button_y_pressed, true);
}

TEST_F(PubSubServiceTest, Publish) {
  PW_NANOPB_TEST_METHOD_CONTEXT(sense::PubSubService, Publish) ctx;
  ctx.service().Init(pubsub_);

  ASSERT_TRUE(pubsub_.Subscribe([this](sense::Event event) {
    events_processed_++;

    if (std::holds_alternative<sense::AirQuality>(event)) {
      total_score_ += std::get<sense::AirQuality>(event).score;
    } else if (std::holds_alternative<sense::ButtonA>(event)) {
      if (std::get<sense::ButtonA>(event).pressed()) {
        button_presses_++;
      }
    } else {
      FAIL() << "Unexpected event type";
    }

    notification_.release();
  }));

  EXPECT_EQ(ctx.call({.which_type = pubsub_Event_button_a_pressed_tag,
                      .type = {.button_a_pressed = true}}),
            pw::OkStatus());
  notification_.acquire();
  EXPECT_EQ(total_score_, 0u);
  EXPECT_EQ(events_processed_, 1u);
  EXPECT_EQ(button_presses_, 1u);

  EXPECT_EQ(ctx.call({.which_type = pubsub_Event_air_quality_tag,
                      .type = {.air_quality = 256u}}),
            pw::OkStatus());
  notification_.acquire();
  EXPECT_EQ(total_score_, 256u);
  EXPECT_EQ(events_processed_, 2u);
  EXPECT_EQ(button_presses_, 1u);

  EXPECT_EQ(ctx.call({.which_type = pubsub_Event_air_quality_tag,
                      .type = {.air_quality = 768u}}),
            pw::OkStatus());
  notification_.acquire();
  EXPECT_EQ(total_score_, 1024u);
  EXPECT_EQ(events_processed_, 3u);
  EXPECT_EQ(button_presses_, 1u);

  EXPECT_EQ(ctx.call({.which_type = pubsub_Event_button_a_pressed_tag,
                      .type = {.button_a_pressed = false}}),
            pw::OkStatus());
  notification_.acquire();
  EXPECT_EQ(total_score_, 1024u);
  EXPECT_EQ(events_processed_, 4u);
  EXPECT_EQ(button_presses_, 1u);

  EXPECT_EQ(ctx.call({.which_type = pubsub_Event_button_a_pressed_tag,
                      .type = {.button_a_pressed = true}}),
            pw::OkStatus());
  notification_.acquire();
  EXPECT_EQ(total_score_, 1024u);
  EXPECT_EQ(events_processed_, 5u);
  EXPECT_EQ(button_presses_, 2u);
}

}  // namespace

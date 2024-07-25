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
  void TearDown() override { worker_.Stop(); }

  sense::TestWorker<> worker_;
  pw::InlineDeque<sense::Event, 4> event_queue_;
  std::array<sense::PubSub::SubscribeCallback, 4> subscribers_buffer_;
  pw::sync::TimedThreadNotification notification_;
  pw::sync::TimedThreadNotification work_queue_start_notification_;
  int events_processed_ = 0;
  float total_voc_ = 0;
  int button_presses_ = 0;
};

TEST_F(PubSubServiceTest, Subscribe) {
  PW_NANOPB_TEST_METHOD_CONTEXT(sense::PubSubService, Subscribe) ctx;
  sense::PubSub pubsub(worker_, event_queue_, subscribers_buffer_);

  ctx.service().Init(pubsub);
  ctx.call({});

  pw::rpc::test::WaitForPackets(ctx.output(), 3, [&pubsub] {
    EXPECT_TRUE(pubsub.Publish(sense::VocSample{.voc_level = 0.25f}));
    EXPECT_TRUE(pubsub.Publish(sense::ButtonB(false)));
    EXPECT_TRUE(pubsub.Publish(sense::ButtonY(true)));
  });

  EXPECT_EQ(ctx.responses().size(), 3u);
  ASSERT_EQ(ctx.responses()[0].which_type, pubsub_Event_voc_level_tag);
  EXPECT_EQ(ctx.responses()[0].type.voc_level, 0.25f);
  ASSERT_EQ(ctx.responses()[1].which_type, pubsub_Event_button_b_pressed_tag);
  EXPECT_EQ(ctx.responses()[1].type.button_b_pressed, false);
  ASSERT_EQ(ctx.responses()[2].which_type, pubsub_Event_button_y_pressed_tag);
  EXPECT_EQ(ctx.responses()[2].type.button_y_pressed, true);
}

TEST_F(PubSubServiceTest, Publish) {
  PW_NANOPB_TEST_METHOD_CONTEXT(sense::PubSubService, Publish) ctx;
  sense::PubSub pubsub(worker_, event_queue_, subscribers_buffer_);

  ctx.service().Init(pubsub);

  pubsub.Subscribe([this](sense::Event event) {
    events_processed_++;

    if (std::holds_alternative<sense::VocSample>(event)) {
      total_voc_ += std::get<sense::VocSample>(event).voc_level;
    } else if (std::holds_alternative<sense::ButtonA>(event)) {
      if (std::get<sense::ButtonA>(event).pressed()) {
        button_presses_++;
      }
    } else {
      FAIL() << "Unexpected event type";
    }

    notification_.release();
  });

  EXPECT_EQ(ctx.call({.which_type = pubsub_Event_button_a_pressed_tag,
                      .type = {.button_a_pressed = true}}),
            pw::OkStatus());
  EXPECT_TRUE(notification_.try_acquire_for(200ms));
  EXPECT_EQ(events_processed_, 1);
  EXPECT_EQ(button_presses_, 1);

  EXPECT_EQ(ctx.call({.which_type = pubsub_Event_voc_level_tag,
                      .type = {.voc_level = 0.25}}),
            pw::OkStatus());
  EXPECT_TRUE(notification_.try_acquire_for(200ms));
  EXPECT_EQ(events_processed_, 2);
  EXPECT_EQ(button_presses_, 1);

  EXPECT_EQ(ctx.call({.which_type = pubsub_Event_voc_level_tag,
                      .type = {.voc_level = 0.75}}),
            pw::OkStatus());
  EXPECT_TRUE(notification_.try_acquire_for(200ms));
  EXPECT_EQ(events_processed_, 3);
  EXPECT_EQ(button_presses_, 1);

  EXPECT_EQ(ctx.call({.which_type = pubsub_Event_button_a_pressed_tag,
                      .type = {.button_a_pressed = false}}),
            pw::OkStatus());
  EXPECT_TRUE(notification_.try_acquire_for(200ms));
  EXPECT_EQ(events_processed_, 4);
  EXPECT_EQ(button_presses_, 1);

  EXPECT_EQ(ctx.call({.which_type = pubsub_Event_button_a_pressed_tag,
                      .type = {.button_a_pressed = true}}),
            pw::OkStatus());
  EXPECT_TRUE(notification_.try_acquire_for(200ms));
  EXPECT_EQ(events_processed_, 5);
  EXPECT_EQ(button_presses_, 2);
}

}  // namespace

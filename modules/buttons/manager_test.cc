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

#include "modules/buttons/manager.h"

#include "modules/worker/test_worker.h"
#include "pw_digital_io/digital_io.h"
#include "pw_status/status.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_unit_test/framework.h"

using ::pw::chrono::SystemClock;
using ::pw::digital_io::DigitalIn;
using ::pw::digital_io::State;
using ::pw::sync::InterruptSpinLock;
using namespace std::literals::chrono_literals;

namespace sense {

constexpr const auto kMaxWaitOnFailedTest = 3s;

using pw::digital_io::DigitalInOut;
using pw::digital_io::State;

class TestDigitalInOut : public DigitalInOut {
 public:
  TestDigitalInOut() : state_(State::kInactive) {}

  void NoisySetState(int settle_iterations, State final_state) {
    std::lock_guard lock(lock_);
    settle_iterations_ = settle_iterations;
    final_state_ = final_state;
  }

 private:
  pw::Status DoEnable(bool) override { return pw::OkStatus(); }
  pw::Result<State> DoGetState() override {
    std::lock_guard lock(lock_);
    if (settle_iterations_ <= 0) {
      state_ = final_state_;
    } else {
      state_ = state_ == State::kActive ? State::kInactive : State::kActive;
      settle_iterations_ -= 1;
    }

    return state_;
  }
  pw::Status DoSetState(State state) override {
    NoisySetState(0, state);
    return pw::OkStatus();
  }

  InterruptSpinLock lock_;
  State state_ PW_GUARDED_BY(lock_) = State::kInactive;
  State final_state_ PW_GUARDED_BY(lock_) = State::kInactive;
  int settle_iterations_ PW_GUARDED_BY(lock_) = 0;
};

// A test harness for writing tests that use pubsub.
class ManagerTest : public ::testing::Test {
 public:
  using PubSub = sense::GenericPubSub<Event>;

 protected:
  virtual void SetUp() override {
    last_event_ = {};
    events_processed_ = 0;

    ASSERT_EQ(pw::OkStatus(), io_a_.SetState(State::kInactive));
    ASSERT_EQ(pw::OkStatus(), io_b_.SetState(State::kInactive));
    ASSERT_EQ(pw::OkStatus(), io_x_.SetState(State::kInactive));
    ASSERT_EQ(pw::OkStatus(), io_y_.SetState(State::kInactive));
  }

  /// Expects that a button was pressed.
  /// If `false`, the test must abort.
  template <typename Event>
  [[nodiscard]] bool AssertPressed(bool pressed = true) {
    bool acquired = notification_.try_acquire_for(kMaxWaitOnFailedTest);
    EXPECT_TRUE(acquired);
    if (!acquired) {
      return false;
    }
    bool has_value = last_event_.has_value();
    EXPECT_TRUE(has_value);
    if (!has_value) {
      return false;
    }
    bool holds_alternative = std::holds_alternative<Event>(last_event_.value());
    EXPECT_TRUE(holds_alternative);
    if (!holds_alternative) {
      return false;
    }
    EXPECT_EQ(std::get<Event>(last_event_.value()).pressed(), pressed);
    if (std::get<Event>(last_event_.value()).pressed() != pressed) {
      return false;
    }
    return true;
  }

  pw::InlineDeque<Event, 4> event_queue_;
  std::array<typename PubSub::Subscriber, 4> subscribers_buffer_;
  std::optional<Event> last_event_;
  int events_processed_ = 0;
  pw::sync::TimedThreadNotification notification_;

  TestDigitalInOut io_a_;
  TestDigitalInOut io_b_;
  TestDigitalInOut io_x_;
  TestDigitalInOut io_y_;
};

TEST(DebounceTest, SingleEdgePropagatesAfterDelay) {
  Debouncer debouncer(State::kInactive);

  // Arbitrary start time.  Debouncer does not sample clock.
  auto time = SystemClock::now();
  EXPECT_EQ(debouncer.UpdateState(time, State::kInactive), State::kInactive);

  // Line stays inactive for 10ms.
  time += 10ms;
  EXPECT_EQ(debouncer.UpdateState(time, State::kInactive), State::kInactive);

  // 10ms later, the line transitions to active but the output of the debouncer
  // remains inactive.
  time += 10ms;
  EXPECT_EQ(debouncer.UpdateState(time, State::kActive), State::kInactive);

  // The output of the debounces remains active 1ms before it's debounce
  // interval.
  EXPECT_EQ(debouncer.UpdateState(time + Debouncer::kDebounceInterval - 1ms,
                                  State::kActive),
            State::kInactive);

  // After the full interval has eleapse, the output state becomes active.
  EXPECT_EQ(debouncer.UpdateState(time + Debouncer::kDebounceInterval,
                                  State::kActive),
            State::kActive);
}

TEST(DebounceTest, TwoEdgesPropagateAfterDelay) {
  Debouncer debouncer(State::kInactive);

  // Arbitrary start time.  Debouncer does not sample clock.
  auto time = SystemClock::now();
  EXPECT_EQ(debouncer.UpdateState(time, State::kInactive), State::kInactive);

  // Signal stays inactive.
  time += 10ms;
  EXPECT_EQ(debouncer.UpdateState(time, State::kInactive), State::kInactive);

  // Signal becomes active and propagates through debouncer correctly.
  time += 10ms;
  EXPECT_EQ(debouncer.UpdateState(time, State::kActive), State::kInactive);
  EXPECT_EQ(debouncer.UpdateState(time + Debouncer::kDebounceInterval - 1ms,
                                  State::kActive),
            State::kInactive);
  EXPECT_EQ(debouncer.UpdateState(time + Debouncer::kDebounceInterval,
                                  State::kActive),
            State::kActive);

  // Signal becomes inactive and propagates through debouncer correctly.
  time += Debouncer::kDebounceInterval + 10ms;
  EXPECT_EQ(debouncer.UpdateState(time, State::kInactive), State::kActive);
  EXPECT_EQ(debouncer.UpdateState(time + Debouncer::kDebounceInterval - 1ms,
                                  State::kInactive),
            State::kActive);
  EXPECT_EQ(debouncer.UpdateState(time + Debouncer::kDebounceInterval,
                                  State::kInactive),
            State::kInactive);
}
TEST(DebounceTest, RapidStateChangesAreDebounced) {
  Debouncer debouncer(State::kInactive);

  // Arbitrary start time.  Debouncer does not sample clock.
  auto time = SystemClock::now();
  EXPECT_EQ(debouncer.UpdateState(time, pw::digital_io::State::kInactive),
            pw::digital_io::State::kInactive);

  // Signal stays inactive.
  time += 10ms;
  EXPECT_EQ(debouncer.UpdateState(time, State::kInactive), State::kInactive);

  // Signal changes onces every ms for 10ms and the output remains inactive.
  for (int i = 0; i < 10; ++i) {
    time += 1ms;
    auto state = i % 2 == 1 ? State::kActive : State::kInactive;
    EXPECT_EQ(debouncer.UpdateState(time, state), State::kInactive);
  }

  // Signal transitions to active a final time an remains steady.
  time += 1ms;
  EXPECT_EQ(debouncer.UpdateState(time, State::kInactive), State::kInactive);
  time += 1ms;
  EXPECT_EQ(debouncer.UpdateState(time, State::kActive), State::kInactive);
  EXPECT_EQ(debouncer.UpdateState(time + Debouncer::kDebounceInterval - 1ms,
                                  State::kActive),
            State::kInactive);
  EXPECT_EQ(debouncer.UpdateState(time + Debouncer::kDebounceInterval,
                                  State::kActive),
            State::kActive);

  // Signal changes onces every ms for 10ms and the output remains active.
  for (int i = 0; i < 10; ++i) {
    time += 1ms;
    auto state = i % 2 == 1 ? State::kActive : State::kInactive;
    EXPECT_EQ(debouncer.UpdateState(time, state), State::kActive);
  }

  // Signal transitions to inactive a final time an remains steady.
  time += 1ms;
  EXPECT_EQ(debouncer.UpdateState(time, State::kActive), State::kActive);
  time += 1ms;
  EXPECT_EQ(debouncer.UpdateState(time, State::kInactive), State::kActive);
  EXPECT_EQ(debouncer.UpdateState(time + Debouncer::kDebounceInterval - 1ms,
                                  State::kInactive),
            State::kActive);
  EXPECT_EQ(debouncer.UpdateState(time + Debouncer::kDebounceInterval,
                                  State::kInactive),
            State::kInactive);
}

TEST(EdgeDetectorTest, EdgesDetected) {
  EdgeDetector edge_detector(State::kInactive);

  // Inactive -> Inactive = None.
  EXPECT_EQ(edge_detector.UpdateState(State::kInactive),
            EdgeDetector::StateChange::kNone);

  // Inactive -> Active = Activate.
  EXPECT_EQ(edge_detector.UpdateState(State::kActive),
            EdgeDetector::StateChange::kActivate);

  // Active -> Active = None.
  EXPECT_EQ(edge_detector.UpdateState(State::kActive),
            EdgeDetector::StateChange::kNone);

  // Active -> Inactive = Deactivate.
  EXPECT_EQ(edge_detector.UpdateState(State::kInactive),
            EdgeDetector::StateChange::kDeactivate);

  // Inactive -> Inactive = None.
  EXPECT_EQ(edge_detector.UpdateState(State::kInactive),
            EdgeDetector::StateChange::kNone);
}

TEST_F(ManagerTest, AllButtonsTurnOnAndOffEvents) {
  sense::TestWorker<> worker;
  PubSub pubsub(worker, event_queue_, subscribers_buffer_);
  ButtonManager manager(io_a_, io_b_, io_x_, io_y_);
  manager.Init(pubsub, worker);

  ASSERT_TRUE(pubsub.Subscribe([this](Event event) {
    last_event_ = event;
    events_processed_ += 1;
    notification_.release();
  }));

  ASSERT_EQ(pw::OkStatus(), io_a_.SetState(State::kActive));
  ASSERT_TRUE(AssertPressed<sense::ButtonA>());

  ASSERT_EQ(pw::OkStatus(), io_b_.SetState(State::kActive));
  ASSERT_TRUE(AssertPressed<sense::ButtonB>());

  ASSERT_EQ(pw::OkStatus(), io_x_.SetState(State::kActive));
  ASSERT_TRUE(AssertPressed<sense::ButtonX>());

  ASSERT_EQ(pw::OkStatus(), io_y_.SetState(State::kActive));
  ASSERT_TRUE(AssertPressed<sense::ButtonY>());

  ASSERT_EQ(pw::OkStatus(), io_a_.SetState(State::kInactive));
  ASSERT_TRUE(AssertPressed<sense::ButtonA>(false));

  ASSERT_EQ(pw::OkStatus(), io_b_.SetState(State::kInactive));
  ASSERT_TRUE(AssertPressed<sense::ButtonB>(false));

  ASSERT_EQ(pw::OkStatus(), io_x_.SetState(State::kInactive));
  ASSERT_TRUE(AssertPressed<sense::ButtonX>(false));

  ASSERT_EQ(pw::OkStatus(), io_y_.SetState(State::kInactive));
  ASSERT_TRUE(AssertPressed<sense::ButtonY>(false));

  worker.Stop();
}

TEST_F(ManagerTest, DebouncingWorksOnNoisyIo) {
  sense::TestWorker<> worker;
  PubSub pubsub(worker, event_queue_, subscribers_buffer_);
  ASSERT_TRUE(pubsub.Subscribe([this](Event event) {
    last_event_ = event;
    events_processed_ += 1;
    notification_.release();
  }));

  ButtonManager manager(io_a_, io_b_, io_x_, io_y_);
  manager.Init(pubsub, worker);

  // Set line active with 10 noisy transition and assert that we only
  // receive one event.
  io_a_.NoisySetState(10, State::kActive);
  ASSERT_TRUE(AssertPressed<sense::ButtonA>());
  EXPECT_EQ(events_processed_, 1);

  // Set line inactive with 10 noisy transition and assert that we only
  // receive one event.
  io_a_.NoisySetState(10, State::kInactive);
  ASSERT_TRUE(AssertPressed<sense::ButtonA>(false));
  EXPECT_EQ(events_processed_, 2);

  worker.Stop();
}
}  // namespace sense

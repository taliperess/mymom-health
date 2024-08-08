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
#pragma once

#include <chrono>

#include "modules/pubsub/pubsub_events.h"
#include "modules/worker/worker.h"
#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono/system_timer.h"
#include "pw_digital_io/digital_io.h"
#include "pw_function/function.h"
#include "pw_status/status.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"

namespace sense {
class Debouncer final {
 public:
  constexpr static pw::chrono::SystemClock::duration kDebounceInterval =
      std::chrono::milliseconds(30);

  Debouncer(pw::digital_io::State initial_state)
      : last_input_(initial_state) {};
  pw::digital_io::State UpdateState(pw::chrono::SystemClock::time_point now,
                                    pw::digital_io::State state);

 private:
  pw::chrono::SystemClock::time_point last_update_ =
      pw::chrono::SystemClock::time_point::min();
  pw::digital_io::State last_input_ = pw::digital_io::State::kInactive;
  pw::digital_io::State output_ = pw::digital_io::State::kInactive;
};

class EdgeDetector final {
 public:
  EdgeDetector(pw::digital_io::State initial_state)
      : current_state_(initial_state) {};

  enum StateChange {
    kNone,
    kActivate,
    kDeactivate,
  };

  StateChange UpdateState(pw::digital_io::State state);

 private:
  pw::digital_io::State current_state_;
};

class Button {
 public:
  Button(pw::digital_io::DigitalIn& io) : io_(io) {
    PW_CHECK_OK(io_.Enable());
  };

  pw::Result<EdgeDetector::StateChange> Sample(
      pw::chrono::SystemClock::time_point now);

 private:
  pw::digital_io::DigitalIn& io_;
  Debouncer debouncer_ = Debouncer(pw::digital_io::State::kInactive);
  EdgeDetector edge_detector_ = EdgeDetector(pw::digital_io::State::kInactive);
};

/// DOCME
class ButtonManager final {
 public:
  constexpr static pw::chrono::SystemClock::duration kSampleInterval =
      std::chrono::milliseconds(10);

  ButtonManager(pw::digital_io::DigitalIn& button_a,
                pw::digital_io::DigitalIn& button_b,
                pw::digital_io::DigitalIn& button_x,
                pw::digital_io::DigitalIn& button_y);
  ~ButtonManager();

  void Init(PubSub& pub_sub, Worker& worker);

  void Start() {
    if (!active_) {
      timer_.InvokeAfter(kSampleInterval);
    }
    active_ = true;
  }

  void Stop() {
    timer_.Cancel();
    active_ = false;
  }

 private:
  Button buttons_[4];

  pw::Status ScheduleSampleCallback();
  void SampleCallback(pw::chrono::SystemClock::time_point);

  template <typename ButtonEvent>
  pw::Status SampleButton(Button& button, pw::chrono::SystemClock::time_point);
  pw::Status SampleButtons(pw::chrono::SystemClock::time_point);

  PubSub* pub_sub_ = nullptr;
  Worker* worker_ = nullptr;
  pw::chrono::SystemTimer timer_;
  bool active_;
};
}  // namespace sense

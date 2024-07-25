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

#include "modules/morse_code/encoder.h"

#include <chrono>
#include <cstdint>

#include "modules/led/monochrome_led_fake.h"
#include "modules/worker/test_worker.h"
#include "pw_containers/vector.h"
#include "pw_status/status.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_thread/sleep.h"
#include "pw_thread/thread.h"
#include "pw_unit_test/framework.h"

namespace sense {

// Test fixtures.

class MorseCodeEncoderTest : public ::testing::Test {
 protected:
  using Event = ::sense::MonochromeLedFake::Event;
  using State = ::sense::MonochromeLedFake::State;

  static constexpr uint32_t kIntervalMs = 10;

  // TODO(b/352327457): Ideally this would use simulated time, but no
  // simulated system timer exists yet. For now, relax the constraint by
  // checking that the LED was in the right state for _at least_ the expected
  // number of intervals. On some platforms, the fake LED is implemented using
  // threads, and may sleep a bit longer.
  MorseCodeEncoderTest()
      : clock_(pw::chrono::VirtualSystemClock::RealClock()) {}

  void Expect(std::string_view msg) {
    auto& events = led_.events();
    auto event = events.begin();

    // Skip until the first "turn on" event is seen, and use it as the starting
    // point.
    while (true) {
      if (event == events.end()) {
        EXPECT_TRUE(msg.empty());
        return;
      }
      if (event->state == State::kActive) {
        break;
      }
      ++event;
    }
    auto start = event->timestamp;
    ++event;

    while (!msg.empty()) {
      size_t off_intervals;
      size_t offset;
      if (msg.substr(1, 2) == "  ") {
        // Word break
        off_intervals = 7;
        offset = 3;

      } else if (msg.substr(1, 1) == " ") {
        // Letter break
        off_intervals = 3;
        offset = 2;

      } else {
        // Symbol break
        off_intervals = 1;
        offset = 1;
      }

      // Check that the LED turns off after the roght amount of time, implying
      /// it was on.
      ASSERT_NE(event, events.end());
      EXPECT_EQ(event->state, State::kInactive);
      if (msg[0] == '.') {
        EXPECT_GE(ToMs(event->timestamp - start), interval_ms_);
      } else if (msg[0] == '-') {
        EXPECT_GE(ToMs(event->timestamp - start), interval_ms_ * 3);
      } else {
        FAIL();
      }
      start = event->timestamp;
      ++event;

      msg = msg.substr(offset);
      if (msg.empty()) {
        break;
      }

      // Check that the LED turns on after the roght amount of time, implying
      /// it was off.
      ASSERT_NE(event, events.end());
      EXPECT_EQ(event->state, State::kActive);
      EXPECT_GE(ToMs(event->timestamp - start), interval_ms_ * off_intervals);
      start = event->timestamp;
      ++event;
    }
    events.clear();
  }

  uint32_t ToMs(pw::chrono::SystemClock::duration interval) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(interval)
        .count();
  }
  /// Waits until the given encoder is idle.
  void SleepUntilDone() {
    auto interval = pw::chrono::SystemClock::for_at_least(
        std::chrono::milliseconds(interval_ms_));
    while (!encoder_.IsIdle()) {
      pw::this_thread::sleep_for(interval);
    }
  }

  void FakeLedOutput(bool turn_on) {
    if (turn_on) {
      led_.TurnOn();
    } else {
      led_.TurnOff();
    }
  }

  pw::chrono::VirtualSystemClock& clock_;
  Encoder encoder_;
  MonochromeLedFake led_;
  uint32_t interval_ms_ = kIntervalMs;
};

// Unit tests.

TEST_F(MorseCodeEncoderTest, EncodeEmpty) {
  TestWorker<> worker;
  encoder_.Init(worker, [this](bool turn_on) { FakeLedOutput(turn_on); });
  EXPECT_EQ(encoder_.Encode("", 1, interval_ms_), pw::OkStatus());
  SleepUntilDone();
  worker.Stop();
  Expect("");
}

TEST_F(MorseCodeEncoderTest, EncodeOneLetter) {
  TestWorker<> worker;
  encoder_.Init(worker, [this](bool turn_on) { FakeLedOutput(turn_on); });
  EXPECT_EQ(encoder_.Encode("E", 1, interval_ms_), pw::OkStatus());
  SleepUntilDone();
  worker.Stop();
  Expect(".");
}

TEST_F(MorseCodeEncoderTest, EncodeOneWord) {
  TestWorker<> worker;
  encoder_.Init(worker, [this](bool turn_on) { FakeLedOutput(turn_on); });
  EXPECT_EQ(encoder_.Encode("PARIS", 1, interval_ms_), pw::OkStatus());
  SleepUntilDone();
  worker.Stop();
  Expect(".--. .- .-. .. ...");
}

TEST_F(MorseCodeEncoderTest, EncodeHelloWorld) {
  TestWorker<> worker;
  encoder_.Init(worker, [this](bool turn_on) { FakeLedOutput(turn_on); });
  EXPECT_EQ(encoder_.Encode("hello world", 1, interval_ms_), pw::OkStatus());
  SleepUntilDone();
  worker.Stop();

  Expect(".... . .-.. .-.. ---  .-- --- .-. .-.. -..");
}

// TODO(b/352327457): Without simulated time, this test is too slow to run every
// case on device.
#ifdef AM_MORSE_CODE_ENCODER_TEST_FULL
TEST_F(MorseCodeEncoderTest, EncodeRepeated) {
  TestWorker<> worker;
  encoder_.Init(worker, [this](bool turn_on) { FakeLedOutput(turn_on); });
  EXPECT_EQ(encoder_.Encode("hello", 2, interval_ms_), pw::OkStatus());
  SleepUntilDone();
  worker.Stop();
  Expect(".... . .-.. .-.. ---  .... . .-.. .-.. ---");
}

TEST_F(MorseCodeEncoderTest, EncodeSlow) {
  TestWorker<> worker;
  encoder_.Init(worker, [this](bool turn_on) { FakeLedOutput(turn_on); });
  interval_ms_ = 25;
  EXPECT_EQ(encoder_.Encode("hello", 1, interval_ms_), pw::OkStatus());
  SleepUntilDone();
  worker.Stop();
  Expect(".... . .-.. .-.. ---");
}

TEST_F(MorseCodeEncoderTest, EncodeConsecutiveWhitespace) {
  TestWorker<> worker;
  encoder_.Init(worker, [this](bool turn_on) { FakeLedOutput(turn_on); });
  EXPECT_EQ(encoder_.Encode("hello    world", 1, interval_ms_), pw::OkStatus());
  SleepUntilDone();
  worker.Stop();
  Expect(".... . .-.. .-.. ---  .-- --- .-. .-.. -..");
}

TEST_F(MorseCodeEncoderTest, EncodeInvalidChars) {
  TestWorker<> worker;
  encoder_.Init(worker, [this](bool turn_on) { FakeLedOutput(turn_on); });
  char s[2];
  s[1] = 0;
  for (char c = 127; c != 0; --c) {
    if (isspace(c) || isalnum(c) || c == '?' || c == '@') {
      continue;
    }
    s[0] = c;
    EXPECT_EQ(encoder_.Encode(s, 1, interval_ms_), pw::OkStatus());
    SleepUntilDone();
    Expect("..--..");
  }
  worker.Stop();
}
#endif  // AM_MORSE_CODE_ENCODER_TEST_FULL

}  // namespace sense

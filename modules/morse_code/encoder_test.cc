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
#include "pw_thread/sleep.h"
#include "pw_thread/thread.h"
#include "pw_unit_test/framework.h"

namespace am {

// Test fixtures.

class MorseCodeEncoderTest : public ::testing::Test {
 protected:
  // TODO(b/352327457): Ideally this would use simulated time, but no
  // simulated system timer exists yet. For now, relax the constraint by
  // checking that the LED was in the right state for _at least_ the expected
  // number of intervals. On some platforms, the fake LED is implemented using
  // threads, and may sleep a bit longer.
  void Expect(bool is_on, size_t num_intervals) {
    const pw::Vector<uint8_t>& actual = led_.GetOutput();
    ASSERT_LT(offset_, actual.size());
    uint8_t encoded = MonochromeLedFake::Encode(is_on, num_intervals);
    EXPECT_GE(actual[offset_], encoded);
    ++offset_;
  }

  void Expect(std::string_view msg) {
    offset_ = 0;
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

      if (msg[0] == '.') {
        Expect(true, 1);
      } else if (msg[0] == '-') {
        Expect(true, 3);
      } else {
        FAIL();
      }
      msg = msg.substr(offset);
      if (msg.empty()) {
        break;
      }
      Expect(false, off_intervals);
    }
    led_.ResetOutput();
  }

  /// Waits until the given encoder is idle.
  void SleepUntilDone() {
    while (!encoder_.IsIdle()) {
      pw::this_thread::sleep_for(led_.interval());
    }
  }

  Encoder encoder_;
  MonochromeLedFake led_;
  size_t offset_ = 0;

 private:
  pw::Vector<char, MonochromeLedFake::kCapacity> output_;
};

// Unit tests.

TEST_F(MorseCodeEncoderTest, EncodeEmpty) {
  TestWorker<> worker;
  encoder_.Init(worker, led_);
  EXPECT_EQ(encoder_.Encode("", 1, led_.interval_ms()), pw::OkStatus());
  SleepUntilDone();
  worker.Stop();
  Expect("");
}

TEST_F(MorseCodeEncoderTest, EncodeHelloWorld) {
  TestWorker<> worker;
  encoder_.Init(worker, led_);
  EXPECT_EQ(encoder_.Encode("hello world", 1, led_.interval_ms()),
            pw::OkStatus());
  SleepUntilDone();
  worker.Stop();

  Expect(".... . .-.. .-.. ---  .-- --- .-. .-.. -..");
}

TEST_F(MorseCodeEncoderTest, EncodeRepeated) {
  TestWorker<> worker;
  encoder_.Init(worker, led_);
  EXPECT_EQ(encoder_.Encode("hello", 2, led_.interval_ms()), pw::OkStatus());
  SleepUntilDone();
  worker.Stop();
  Expect(".... . .-.. .-.. ---  .... . .-.. .-.. ---");
}

TEST_F(MorseCodeEncoderTest, EncodeSlow) {
  TestWorker<> worker;
  encoder_.Init(worker, led_);
  led_.set_interval_ms(20);
  EXPECT_EQ(encoder_.Encode("hello", 1, led_.interval_ms()), pw::OkStatus());
  SleepUntilDone();
  worker.Stop();
  Expect(".... . .-.. .-.. ---");
}

TEST_F(MorseCodeEncoderTest, EncodeConsecutiveWhitespace) {
  TestWorker<> worker;
  encoder_.Init(worker, led_);
  EXPECT_EQ(encoder_.Encode("hello    world", 1, led_.interval_ms()),
            pw::OkStatus());
  SleepUntilDone();
  worker.Stop();
  Expect(".... . .-.. .-.. ---  .-- --- .-. .-.. -..");
}

TEST_F(MorseCodeEncoderTest, EncodeInvalidChars) {
  TestWorker<> worker;
  encoder_.Init(worker, led_);
  char s[2];
  s[1] = 0;
  for (char c = 127; c != 0; --c) {
    if (isspace(c) || isalnum(c) || c == '?' || c == '@') {
      continue;
    }
    s[0] = c;
    EXPECT_EQ(encoder_.Encode(s, 1, led_.interval_ms()), pw::OkStatus());
    SleepUntilDone();
    Expect("..--..");
  }
  worker.Stop();
}

}  // namespace am

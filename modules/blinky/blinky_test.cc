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

#include "modules/blinky/blinky.h"

#include "modules/led/monochrome_led_fake.h"
#include "modules/led/polychrome_led_fake.h"
#include "modules/worker/test_worker.h"
#include "pw_thread/sleep.h"
#include "pw_unit_test/framework.h"

namespace am {

// Test fixtures.

class BlinkyTest : public ::testing::Test {
 protected:
  // TODO(b/352327457): Ideally this would use simulated time, but no
  // simulated system timer exists yet. For now, relax the constraint by
  // checking that the LED was in the right state for _at least_ the expected
  // number of intervals. On some platforms, the fake LED is implemented using
  // threads, and may sleep a bit longer.
  void Expect(bool is_on, size_t num_intervals) {
    const pw::Vector<uint8_t>& actual = monochrome_led_.GetOutput();
    ASSERT_LT(offset_, actual.size());
    uint8_t encoded = MonochromeLedFake::Encode(is_on, num_intervals);
    EXPECT_GE(actual[offset_], encoded);
    ++offset_;
  }

  MonochromeLedFake monochrome_led_;
  PolychromeLedFake polychrome_led_;
  size_t offset_ = 0;
};

// Unit tests.

TEST_F(BlinkyTest, Toggle) {
  TestWorker<> worker;
  Blinky blinky;
  blinky.Init(worker, monochrome_led_, polychrome_led_);

  blinky.Toggle();
  pw::this_thread::sleep_for(monochrome_led_.interval() * 1);
  blinky.Toggle();
  pw::this_thread::sleep_for(monochrome_led_.interval() * 2);
  blinky.Toggle();
  pw::this_thread::sleep_for(monochrome_led_.interval() * 3);
  blinky.Toggle();
  worker.Stop();

  Expect(true, 1);
  Expect(false, 2);
  Expect(true, 3);
}

TEST_F(BlinkyTest, BlinkTwice) {
  TestWorker worker;
  Blinky blinky;
  blinky.Init(worker, monochrome_led_, polychrome_led_);
  EXPECT_EQ(blinky.Blink(2, 1), pw::OkStatus());
  while (!blinky.IsIdle()) {
    pw::this_thread::sleep_for(monochrome_led_.interval());
  }
  worker.Stop();

  // Since the fake LED only records completed intervals, we need to blink twice
  // to see both "on" and "off".
  Expect(true, 1);
  Expect(false, 1);
  Expect(true, 1);
}

TEST_F(BlinkyTest, BlinkMany) {
  TestWorker<> worker;
  Blinky blinky;
  blinky.Init(worker, monochrome_led_, polychrome_led_);
  EXPECT_EQ(blinky.Blink(100, 1), pw::OkStatus());
  while (!blinky.IsIdle()) {
    pw::this_thread::sleep_for(monochrome_led_.interval());
  }
  worker.Stop();

  // Every "on" and "off" is recorded, except the final one.
  EXPECT_EQ(monochrome_led_.GetOutput().size(), 199U);
}

TEST_F(BlinkyTest, BlinkSlow) {
  TestWorker<> worker;
  Blinky blinky;
  blinky.Init(worker, monochrome_led_, polychrome_led_);
  EXPECT_EQ(blinky.Blink(2, 32), pw::OkStatus());
  while (!blinky.IsIdle()) {
    pw::this_thread::sleep_for(monochrome_led_.interval());
  }
  worker.Stop();

  Expect(true, 32);
  Expect(false, 32);
  Expect(true, 32);
}

}  // namespace am

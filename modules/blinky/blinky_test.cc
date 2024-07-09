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
#include "modules/worker/test_worker.h"
#include "pw_thread/sleep.h"
#include "pw_unit_test/framework.h"

namespace am {

// Test fixtures.

class BlinkyTest : public ::testing::Test {
 protected:
  MonochromeLedFake led_;
};

// Unit tests.

TEST_F(BlinkyTest, Toggle) {
  TestWorker<> worker;
  Blinky blinky;
  blinky.Init(worker, led_);

  blinky.Toggle();
  pw::this_thread::sleep_for(led_.interval() * 1);
  blinky.Toggle();
  pw::this_thread::sleep_for(led_.interval() * 2);
  blinky.Toggle();
  pw::this_thread::sleep_for(led_.interval() * 3);
  blinky.Toggle();
  worker.Stop();

  pw::Vector<uint8_t, 3> expected;
  expected.push_back(MonochromeLedFake::Encode(true, 1));
  expected.push_back(MonochromeLedFake::Encode(false, 2));
  expected.push_back(MonochromeLedFake::Encode(true, 3));
  EXPECT_EQ(led_.GetOutput(), expected);
}

TEST_F(BlinkyTest, BlinkTwice) {
  TestWorker worker;
  Blinky blinky;
  blinky.Init(worker, led_);
  EXPECT_EQ(blinky.Blink(2, 1), pw::OkStatus());
  while (!blinky.IsIdle()) {
    pw::this_thread::sleep_for(led_.interval());
  }
  worker.Stop();

  // Since the fake LED only records completed intervals, we need to blink twice
  // to see both "on" and "off".
  pw::Vector<uint8_t, 3> expected;
  expected.push_back(MonochromeLedFake::Encode(true, 1));
  expected.push_back(MonochromeLedFake::Encode(false, 1));
  expected.push_back(MonochromeLedFake::Encode(true, 1));
  EXPECT_EQ(led_.GetOutput(), expected);
}

TEST_F(BlinkyTest, BlinkMany) {
  TestWorker<> worker;
  Blinky blinky;
  blinky.Init(worker, led_);
  EXPECT_EQ(blinky.Blink(100, 1), pw::OkStatus());
  while (!blinky.IsIdle()) {
    pw::this_thread::sleep_for(led_.interval());
  }
  worker.Stop();

  // Every "on" and "off" is recorded, except the final one.
  EXPECT_EQ(led_.GetOutput().size(), 199U);
}

TEST_F(BlinkyTest, BlinkSlow) {
  TestWorker<> worker;
  Blinky blinky;
  blinky.Init(worker, led_);
  EXPECT_EQ(blinky.Blink(2, 32), pw::OkStatus());
  while (!blinky.IsIdle()) {
    pw::this_thread::sleep_for(led_.interval());
  }
  worker.Stop();

  pw::Vector<uint8_t, 3> expected;
  expected.push_back(MonochromeLedFake::Encode(true, 32));
  expected.push_back(MonochromeLedFake::Encode(false, 32));
  expected.push_back(MonochromeLedFake::Encode(true, 32));
  EXPECT_EQ(led_.GetOutput(), expected);
}

}  // namespace am

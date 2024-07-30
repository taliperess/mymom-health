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

#include "modules/stats/simple_moving_average.h"

#include <cmath>
#include <cstddef>

#include "pw_unit_test/framework.h"

namespace {

TEST(IntegerDivisionRoundNearest, MatchesRoundedFloatDivisionSigned) {
  for (int dividend = -300; dividend <= 300; ++dividend) {
    for (int divisor = -17; divisor <= 17; ++divisor) {
      if (divisor == 0) {
        continue;  // Don't try to divide by 0 -- it's a bad idea.
      }
      const float float_quotient = static_cast<float>(dividend) / divisor;
      ASSERT_EQ(sense::IntegerDivisionRoundNearest(dividend, divisor),
                static_cast<int>(std::lround(float_quotient)));
    }
  }
}

TEST(IntegerDivisionRoundNearest, MatchesRoundedFloatDivisionUnsigned) {
  for (int i = 0; i <= 255; ++i) {
    unsigned char dividend = static_cast<unsigned char>(i);
    for (int j = 0; j <= 255; ++j) {
      unsigned char divisor = static_cast<unsigned char>(j);
      if (divisor == 0) {
        continue;  // Don't try to divide by 0 -- it's a bad idea.
      }
      const float float_quotient = static_cast<float>(dividend) / divisor;
      ASSERT_EQ(sense::IntegerDivisionRoundNearest(dividend, divisor),
                static_cast<int>(std::lround(float_quotient)));
    }
  }
}

// Non-constexpr function to cause a consteval function to fail.
void TestFailed() {}

// Constexpr version of EXPECT_EQ.
consteval void ExpectEq(int lhs, int rhs) {
  if (lhs != rhs) {
    TestFailed();
  }
}

// Simple moving average test with easily inspectable values.
static_assert([] {
  sense::IntegerSimpleMovingAverager<int, 5> avg;
  ExpectEq(avg.Average(), 0);
  avg.Update(0);
  ExpectEq(avg.Average(), 0);
  avg.Update(100);
  ExpectEq(avg.Average(), 20);
  avg.Update(100);
  ExpectEq(avg.Average(), 40);
  avg.Update(100);
  ExpectEq(avg.Average(), 60);
  avg.Update(100);
  ExpectEq(avg.Average(), 80);
  avg.Update(100);
  ExpectEq(avg.Average(), 100);
  avg.Update(0);
  ExpectEq(avg.Average(), 80);
  avg.Update(200);
  ExpectEq(avg.Average(), 100);
  return true;
}());

// Test rounding unsigned values where the sum is too large for the sample type.
static_assert([] {
  sense::IntegerSimpleMovingAverager<uint8_t, 3> avg;
  avg.Update(100);
  ExpectEq(avg.Average(), 33);  // 3.333
  avg.Update(100);
  ExpectEq(avg.Average(), 67);   // 6.667
  avg.Update(100);               // sum overflows uint8_t
  ExpectEq(avg.Average(), 100);  // 100.0
  avg.Update(100);
  ExpectEq(avg.Average(), 100);  // 100.0
  return true;
}());

// Test rounding with positive and negative samples.
static_assert([] {
  sense::IntegerSimpleMovingAverager<int, 4> avg;
  avg.Update(11);                // sum: 11
  ExpectEq(avg.Average(), 3);    // average: 2.75
  avg.Update(-100);              // sum: -89
  ExpectEq(avg.Average(), -22);  // average: -22.25
  avg.Update(-50);               // sum: -139
  ExpectEq(avg.Average(), -35);  // average: -34.75
  avg.Update(-76);               // sum: -139
  ExpectEq(avg.Average(), -54);  // average: -53.75
  avg.Update(40);                // sum: -186
  ExpectEq(avg.Average(), -47);  // average: -46.5
  avg.Update(-51);               // sum: -137
  ExpectEq(avg.Average(), -34);  // average: -34.25
  avg.Update(201);               // sum: 114
  ExpectEq(avg.Average(), 29);   // average: 28.5
  return true;
}());

// Test initialization.
static_assert([] {
  sense::IntegerSimpleMovingAverager<int, 5> avg(3);
  ExpectEq(avg.Average(), 3);
  return true;
}());

}  // namespace

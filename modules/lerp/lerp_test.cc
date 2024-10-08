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

#include "modules/lerp/lerp.h"

#include "pw_unit_test/framework.h"

TEST(LerpTest, ZeroFractionReturnsA) {
  EXPECT_EQ(sense::Lerp(0, 20, 0, 10), 0);
}
TEST(LerpTest, OneFractionReturnsB) {
  EXPECT_EQ(sense::Lerp(0, 20, 10, 10), 20);
}
TEST(LerpTest, HalfFractionReturnsHalfwayPoint) {
  EXPECT_EQ(sense::Lerp(0, 20, 5, 10), 10);
}
TEST(LerpTest, BLessThanAHandledCorrectly) {
  EXPECT_EQ(sense::Lerp(20, 0, 5, 10), 10);
}

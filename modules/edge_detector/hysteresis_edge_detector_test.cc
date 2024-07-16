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

#include "modules/edge_detector/hysteresis_edge_detector.h"

#include <optional>

#include "modules/edge_detector/pubsub.h"  // include as compilation test
#include "pw_unit_test/framework.h"

namespace {

TEST(HysteresisEdgeDetector, StartLow) {
  am::HysteresisEdgeDetector<uint16_t> edge_detector(10, 10000);

  EXPECT_EQ(edge_detector.Update(123), am::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(0), am::Edge::kNone);  // starts low
  EXPECT_EQ(edge_detector.Update(9999), am::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(10000), am::Edge::kRising);
  EXPECT_EQ(edge_detector.Update(10001), am::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(500), am::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(1), am::Edge::kFalling);
}

TEST(HysteresisEdgeDetector, StartHigh) {
  am::HysteresisEdgeDetector<uint16_t> edge_detector(100, 200);

  EXPECT_EQ(edge_detector.Update(101), am::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(199), am::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(101), am::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(200), am::Edge::kNone);  // starts high
  EXPECT_EQ(edge_detector.Update(101), am::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(100), am::Edge::kFalling);
  EXPECT_EQ(edge_detector.Update(199), am::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(99), am::Edge::kNone);
}

TEST(HysteresisEdgeDetector, ImmediateFallingEdge) {
  am::HysteresisEdgeDetector<uint16_t> edge_detector(100, 200);

  EXPECT_EQ(edge_detector.Update(200), am::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(100), am::Edge::kFalling);
}

TEST(HysteresisEdgeDetector, ImmediateRisingEdge) {
  am::HysteresisEdgeDetector<uint16_t> edge_detector(100, 200);

  EXPECT_EQ(edge_detector.Update(100), am::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(200), am::Edge::kRising);
}

TEST(HysteresisEdgeDetector, ChangingThresholdResetsState) {
  am::HysteresisEdgeDetector<uint16_t> edge_detector(100, 200);

  EXPECT_EQ(edge_detector.Update(0), am::Edge::kNone);
  edge_detector.set_high_threshold(100);
  EXPECT_EQ(edge_detector.Update(200), am::Edge::kNone);
  edge_detector.set_low_threshold(0);
  EXPECT_EQ(edge_detector.Update(0), am::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(100), am::Edge::kRising);
  EXPECT_EQ(edge_detector.Update(1), am::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(0), am::Edge::kFalling);
}

}  // namespace

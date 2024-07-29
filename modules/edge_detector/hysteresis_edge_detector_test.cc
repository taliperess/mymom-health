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
  sense::HysteresisEdgeDetector<uint16_t> edge_detector(10, 10000);

  EXPECT_EQ(edge_detector.Update(123), sense::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(0), sense::Edge::kNone);  // starts low
  EXPECT_EQ(edge_detector.Update(9999), sense::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(10000), sense::Edge::kRising);
  EXPECT_EQ(edge_detector.Update(10001), sense::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(500), sense::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(1), sense::Edge::kFalling);
}

TEST(HysteresisEdgeDetector, StartHigh) {
  sense::HysteresisEdgeDetector<uint16_t> edge_detector(100, 200);

  EXPECT_EQ(edge_detector.Update(101), sense::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(199), sense::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(101), sense::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(200), sense::Edge::kNone);  // starts high
  EXPECT_EQ(edge_detector.Update(101), sense::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(100), sense::Edge::kFalling);
  EXPECT_EQ(edge_detector.Update(199), sense::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(99), sense::Edge::kNone);
}

TEST(HysteresisEdgeDetector, ImmediateFallingEdge) {
  sense::HysteresisEdgeDetector<uint16_t> edge_detector(100, 200);

  EXPECT_EQ(edge_detector.Update(200), sense::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(100), sense::Edge::kFalling);
}

TEST(HysteresisEdgeDetector, ImmediateRisingEdge) {
  sense::HysteresisEdgeDetector<uint16_t> edge_detector(100, 200);

  EXPECT_EQ(edge_detector.Update(100), sense::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(200), sense::Edge::kRising);
}

TEST(HysteresisEdgeDetector, ChangingThresholdResetsState) {
  sense::HysteresisEdgeDetector<uint16_t> edge_detector(100, 200);

  EXPECT_EQ(edge_detector.Update(0), sense::Edge::kNone);
  edge_detector.set_low_and_high_thresholds(100, 100);
  EXPECT_EQ(edge_detector.Update(200), sense::Edge::kNone);
  edge_detector.set_low_and_high_thresholds(0, 100);
  EXPECT_EQ(edge_detector.Update(0), sense::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(100), sense::Edge::kRising);
  EXPECT_EQ(edge_detector.Update(1), sense::Edge::kNone);
  EXPECT_EQ(edge_detector.Update(0), sense::Edge::kFalling);
}

}  // namespace

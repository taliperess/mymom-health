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

#include "pw_assert/assert.h"

namespace sense {

enum class Edge { kNone, kRising, kFalling };

namespace internal {

// Non-templated base class for HysteresisEdgeDetector to share logic across
// instantiations and so pw_log can be used.
class BaseHysteresisEdgeDetector {
 protected:
  explicit constexpr BaseHysteresisEdgeDetector() = default;

  /// Processes a "low" sample, which is equal to or below the low threshold.
  Edge UpdateLowSample() { return UpdateState(kLowSample); }

  /// Processes a "high" sample, which is above the high threshold.
  Edge UpdateHighSample() { return UpdateState(kHighSample); }

  void ResetState() { state_ = kInitial; }

 private:
  enum Event {
    kLowSample,
    kHighSample,
  };

  Edge UpdateState(Event event);

  enum {
    kInitial,
    kLow,
    kHigh,
  } state_ = kInitial;
};

}  // namespace internal

/// `HysteresisEdgeDetector` adds hysteresis to a noisy analog signal and
/// converts it to a digital signal. It reports rising and falling edges, when
/// samples cross above an upper threshold or below a lower threshold.
///
/// As an example, `HysteresisEdgeDetector` could be used with a proximity
/// sensor, which produces noisy quantized analog samples. This class converts
/// these samples to e.g. "near" or "far" signals for the rest of the system to
/// consume.
///
/// Thresholds are inclusive, so it is always possible to cross them. If the
/// thresholds are equal, samples with that value are considered to be below the
/// low threshold.
///
/// This class is NOT thread safe. It must only be used from one thread or have
/// external synchronization.
template <typename Sample>
class HysteresisEdgeDetector : public internal::BaseHysteresisEdgeDetector {
 public:
  explicit constexpr HysteresisEdgeDetector(Sample low_threshold,
                                            Sample high_threshold)
      : low_threshold_(low_threshold), high_threshold_(high_threshold) {
    PW_ASSERT(low_threshold_ <= high_threshold_);
  }

  /// Sets the low and high thresholds, inclusive. Resets the internal state.
  void set_low_and_high_thresholds(Sample low_threshold,
                                   Sample high_threshold) {
    PW_ASSERT(low_threshold_ <= high_threshold_);
    low_threshold_ = low_threshold;
    high_threshold_ = high_threshold;
    ResetState();
  }

  /// Adds a new sample to the edge detector. Returns whether the sample crossed
  /// below the lower threshold or above the upper threshold.
  [[nodiscard]] Edge Update(Sample sample) {
    if (sample <= low_threshold_) {
      return UpdateLowSample();
    }
    if (sample >= high_threshold_) {
      return UpdateHighSample();
    }
    return Edge::kNone;
  }

 private:
  Sample low_threshold_;
  Sample high_threshold_;
};

}  // namespace sense

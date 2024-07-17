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

#include <span>

#include "modules/pubsub/pubsub_events.h"
#include "modules/worker/worker.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono/system_timer.h"

namespace am {
namespace testing {
/// Class that can be declared in tests that has access to private members
/// of ColorRotationManager.
class ColorRotationManagerTester;
}  // namespace testing

/// Manage the sequencing of rotating through a set of colors
class ColorRotationManager {
 public:
  /// A Color rotation step.
  struct Step {
    /// Red value for the beginning of the step
    uint8_t r;

    /// Red value for the beginning of the step
    uint8_t g;

    /// Red value for the beginning of the step
    uint8_t b;

    /// Number of cycles to spend transitioning between this step and the next.
    uint16_t num_cycles;
  };

  constexpr static pw::chrono::SystemClock::duration kStepInterval =
      std::chrono::milliseconds(20);

  /// Construct a ColorRoationManager with a given set of steps.
  ///
  /// @param steps The steps in the color rotation.
  ColorRotationManager(pw::span<Step> steps);

  /// Inject pub_sub and worker instances in to the manager.
  void Init(PubSub& pub_sub, Worker& worker);

  /// Start the manager's periodic execution.
  void Start();

  /// Stop the manager's periodic execution.
  void Stop();

 private:
  void UpdateCallback(pw::chrono::SystemClock::time_point now);
  void Update();

  Step& CurrentStep();
  Step& NextStep();

  pw::span<Step> steps_;
  size_t current_step_ = 0;
  uint16_t step_cycle_ = 0;
  bool is_running_ = false;

  PubSub* pub_sub_ = nullptr;
  Worker* worker_ = nullptr;
  pw::chrono::SystemTimer timer_;

  friend testing::ColorRotationManagerTester;
};
}  // namespace am

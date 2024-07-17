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

#include "modules/color_rotation/manager.h"

#include "modules/lerp/lerp.h"
#include "pw_assert/check.h"

using pw::chrono::SystemClock;

namespace am {

ColorRotationManager::ColorRotationManager(const pw::span<const Step> steps,
                                           PubSub& pub_sub,
                                           Worker& worker)
    : steps_(steps),
      pub_sub_(pub_sub),
      worker_(worker),
      timer_(pw::bind_member<&ColorRotationManager::UpdateCallback>(this)) {}

void ColorRotationManager::Start() {
  // Start the periodic update callbacks.
  is_running_ = true;
  timer_.InvokeAfter(kStepInterval);
}

void ColorRotationManager::Stop() { is_running_ = false; }

void ColorRotationManager::UpdateCallback(SystemClock::time_point /* now */) {
  worker_.RunOnce([this]() {
    Update();
    // Reschedule our periodic callback, if running.
    if (is_running_) {
      timer_.InvokeAfter(kStepInterval);
    }
  });
}

void ColorRotationManager::Update() {
  const Step& current = CurrentStep();
  const Step& next = NextStep();
  uint8_t r = Lerp(current.r, next.r, step_cycle_, current.num_cycles);
  uint8_t g = Lerp(current.g, next.g, step_cycle_, current.num_cycles);
  uint8_t b = Lerp(current.b, next.b, step_cycle_, current.num_cycles);

  // Advance step cycle counter.
  step_cycle_ += 1;

  // Detect end of current step and advance step index.
  if (step_cycle_ >= current.num_cycles) {
    step_cycle_ = 0;
    current_step_ += 1;
    if (current_step_ >= steps_.size()) {
      current_step_ = 0;
    }
  }

  pub_sub_.Publish(LedValueColorRotationMode(r, g, b));
}

const ColorRotationManager::Step& ColorRotationManager::CurrentStep() {
  return steps_[current_step_];
}

const ColorRotationManager::Step& ColorRotationManager::NextStep() {
  size_t next_step = current_step_ + 1;
  if (next_step >= steps_.size()) {
    next_step = 0;
  }
  return steps_[next_step];
}

}  // namespace am

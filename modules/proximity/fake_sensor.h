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

#include "modules/proximity/sensor.h"

namespace sense {

/// Fake proximity sensor.
class FakeProximitySensor final : public ProximitySensor {
 public:
  constexpr FakeProximitySensor() = default;

  void set_sample(uint16_t sample) { sample_ = sample; }

  void set_sample_error(pw::Status error) {
    sample_ = pw::Result<uint16_t>(error);
  }

 private:
  pw::Status DoEnableProximitySensor() override { return pw::OkStatus(); }

  pw::Status DoDisableProximitySensor() override { return pw::OkStatus(); }

  pw::Result<uint16_t> DoReadProxSample() override { return sample_; }

  pw::Result<uint16_t> sample_;
};

}  // namespace sense

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

#include <chrono>
#include <utility>

#include "modules/proximity/sensor.h"
#include "pw_chrono/system_clock.h"
#include "pw_i2c/address.h"
#include "pw_i2c/initiator.h"
#include "pw_i2c/register_device.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace am {

/// Basic driver for the LTR559 ambient light and proximity sensor.
class Ltr559LightAndProxSensor {
 public:
  // Minimum delay after power on.
  static constexpr pw::chrono::SystemClock::duration kPowerOnDelay =
      pw::chrono::SystemClock::for_at_least(std::chrono::milliseconds(100));
  // Max time after active mode for first sample.
  static constexpr pw::chrono::SystemClock::duration kActiveModeDelay =
      pw::chrono::SystemClock::for_at_least(std::chrono::milliseconds(10));

  Ltr559LightAndProxSensor(pw::i2c::Initiator& i2c_initiator,
                           pw::chrono::SystemClock::duration timeout =
                               std::chrono::milliseconds(100));

  pw::Status EnableProximity() {
    return device_.WriteRegister(kPsContrAddress, std::byte{0x03}, timeout_);
  }

  pw::Status DisableProximity() {
    return device_.WriteRegister(kPsContrAddress, std::byte{0}, timeout_);
  }

  pw::Result<uint16_t> ReadProximitySample();

 private:
  static constexpr uint8_t kPsContrAddress = 0x81;

  pw::i2c::Initiator& i2c_initiator_;
  pw::i2c::RegisterDevice device_;
  pw::chrono::SystemClock::duration timeout_;
};

// LTR559 that inmplements the generic ProximitySensor interface.
class Ltr559ProximitySensor final : public ProximitySensor {
 public:
  template <typename... Args>
  explicit Ltr559ProximitySensor(Args&&... args)
      : sensor_(std::forward<Args>(args)...) {}

 private:
  pw::Status DoEnable() override { return sensor_.EnableProximity(); }

  pw::Status DoDisable() override { return sensor_.DisableProximity(); }

  pw::Result<uint16_t> DoReadSample() override;

  Ltr559LightAndProxSensor sensor_;
};

}  // namespace am

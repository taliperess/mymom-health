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

#include "modules/light/sensor.h"
#include "modules/proximity/sensor.h"
#include "pw_chrono/system_clock.h"
#include "pw_i2c/address.h"
#include "pw_i2c/initiator.h"
#include "pw_i2c/register_device.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace sense {

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

  /// Enables the ambient light sensor.
  pw::Status EnableLight() {
    return device_.WriteRegister(kAlsContrAddress, std::byte{0x01}, timeout_);
  }

  pw::Status DisableLight() {
    return device_.WriteRegister(kAlsContrAddress, std::byte{0}, timeout_);
  }

  /// Enables the proximity sensor.
  pw::Status EnableProximity() {
    return device_.WriteRegister(kPsContrAddress, std::byte{0x03}, timeout_);
  }

  pw::Status DisableProximity() {
    return device_.WriteRegister(kPsContrAddress, std::byte{0}, timeout_);
  }

  struct Info {
    uint8_t part_id;
    uint8_t manufacturer_id;
  };

  pw::Result<Info> ReadIds();

  pw::Result<uint16_t> ReadProximitySample();

  pw::Result<float> ReadLightSampleLux();

 private:
  static constexpr uint8_t kAlsContrAddress = 0x80;
  static constexpr uint8_t kPsContrAddress = 0x81;

  // 0x86: PART_ID
  // 0x87: MANUFAC_ID
  static constexpr uint8_t kPartIdAddress = 0x86;

  // 0x88-89: ALS_DATA_CH1
  // 0x8A-8B: ALS_DATA_CH0
  static constexpr uint8_t kAlsDataCh1Address = 0x88;

  static constexpr int kDefaultIntegrationTimeMillis = 100;
  static constexpr int kDefaultGain = 1;

  pw::i2c::Initiator& i2c_initiator_;
  pw::i2c::RegisterDevice device_;
  pw::chrono::SystemClock::duration timeout_;
};

// LTR559 that implements the generic ProximitySensor and AmbientLightSensor
// interfaces.
class Ltr559ProxAndLightSensorImpl final : public AmbientLightSensor,
                                           public ProximitySensor {
 public:
  template <typename... Args>
  explicit Ltr559ProxAndLightSensorImpl(Args&&... args)
      : sensor_(std::forward<Args>(args)...) {}

 private:
  pw::Status DoEnableProximitySensor() override {
    return sensor_.EnableProximity();
  }

  pw::Status DoDisableProximitySensor() override {
    return sensor_.DisableProximity();
  }

  pw::Status DoEnableLightSensor() override { return sensor_.EnableLight(); }

  pw::Status DoDisableLightSensor() override { return sensor_.DisableLight(); }

  pw::Result<uint16_t> DoReadProxSample() override;

  pw::Result<float> DoReadLightSampleLux() override {
    return sensor_.ReadLightSampleLux();
  }

  Ltr559LightAndProxSensor sensor_;
};

}  // namespace sense

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

#define PW_LOG_LEVEL PW_LOG_LEVEL_INFO

#include "device/ltr559_light_and_prox_sensor.h"

#include "pw_log/log.h"

namespace sense {

Ltr559LightAndProxSensor::Ltr559LightAndProxSensor(
    pw::i2c::Initiator& i2c_initiator,
    pw::chrono::SystemClock::duration timeout)
    : i2c_initiator_(i2c_initiator),
      device_(i2c_initiator_,
              pw::i2c::Address::SevenBit<0x23>(),
              pw::endian::little,
              pw::endian::little,
              pw::i2c::RegisterAddressSize::k1Byte),
      timeout_(timeout) {}

pw::Result<uint16_t> Ltr559LightAndProxSensor::ReadProximitySample() {
  // 11-bit samples in PS_DATA_0 (0x8D) and PS_DATA_1 (0x8E), little-endian.
  static constexpr uint8_t kPsData0 = 0x8D;
  PW_TRY_ASSIGN(uint16_t sample, device_.ReadRegister16(kPsData0, timeout_));
  return sample & 0x3FFu;  // mask to the 11-bit sample
}

pw::Result<uint16_t> Ltr559ProximitySensor::DoReadSample() {
  // Readings are 11-bit unsigned integers. Scale them to 16 bits.
  PW_TRY_ASSIGN(uint16_t raw_sample, sensor_.ReadProximitySample());
  PW_LOG_DEBUG("LTR-559 sample: %4hu (0x%4hx), scaled: %5u",
               raw_sample,
               raw_sample,
               (raw_sample << 5));
  return raw_sample << 5;
}

}  // namespace sense

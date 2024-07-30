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
namespace {

// Constants from the manufacturer used to convert the ambient light sensor's
// two ADC channels to lux values.
constexpr int kChannel0Constants[] = {17743, 42785, 5926, 0};
constexpr int kChannel1Constants[] = {-11059, 19548, -1185, 0};

}  // namespace

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

pw::Result<float> Ltr559LightAndProxSensor::ReadLightSampleLux() {
  uint16_t channels_1_0_samples[2] = {};
  const auto& [channel_1, channel_0] = channels_1_0_samples;
  PW_TRY(device_.ReadRegisters16(
      kAlsDataCh1Address, channels_1_0_samples, timeout_));

  // Calculate the lux from the two channels based on a formula from the
  // manufacturer.
  const int ratio = (channel_1 + channel_0 == 0)
                        ? 101
                        : (channel_1 * 100 / (channel_1 + channel_0));
  const int index = ratio < 45 ? 0 : ratio < 64 ? 1 : ratio < 85 ? 2 : 3;

  float lux = channel_0 * kChannel0Constants[index] -
              channel_1 * kChannel1Constants[index];
  lux /= (kDefaultIntegrationTimeMillis / 100);
  lux /= kDefaultGain;
  lux /= 10000;
  return lux;
}

pw::Result<Ltr559LightAndProxSensor::Info> Ltr559LightAndProxSensor::ReadIds() {
  uint8_t ids[2];
  PW_TRY(device_.ReadRegisters8(kPartIdAddress, ids, timeout_));
  return Info{.part_id = ids[0], .manufacturer_id = ids[1]};
}

pw::Result<uint16_t> Ltr559ProxAndLightSensorImpl::DoReadProxSample() {
  // Readings are 11-bit unsigned integers. Scale them to 16 bits.
  PW_TRY_ASSIGN(uint16_t raw_sample, sensor_.ReadProximitySample());
  PW_LOG_DEBUG("LTR-559 sample: %4hu (0x%4hx), scaled: %5u",
               raw_sample,
               raw_sample,
               (raw_sample << 5));
  return raw_sample << 5;
}

}  // namespace sense

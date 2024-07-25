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

#include "pw_result/result.h"

namespace sense {

/// Represents a proximity sensor.
class ProximitySensor {
 public:
  /// Starts the sensor so that ReadSample() will return data at the device's
  /// configured interval.
  pw::Status Enable() { return DoEnable(); }

  /// Stops the sensor. ReadSample() will no longer return new samples.
  pw::Status Disable() { return DoDisable(); }

  /// Reads a sample. Sample values range from 0 to 65535. Proximity sensors
  /// must scale their values to span this range.
  ///
  /// Proximity sensors may not be capable of reporting reliable, calibrated
  /// distances. Readings may vary significantly depending on the materials
  /// involved. Users should characterize proximity sensors in their desired use
  /// case to understand these values.
  virtual pw::Result<uint16_t> ReadSample() { return DoReadSample(); }

 protected:
  // Prohibit polymorphic destruction for now.
  ~ProximitySensor() = default;

 private:
  virtual pw::Status DoEnable() = 0;
  virtual pw::Status DoDisable() = 0;
  virtual pw::Result<uint16_t> DoReadSample() = 0;
};

}  // namespace sense

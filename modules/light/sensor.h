
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

/// Represents an ambient light sensor.
class AmbientLightSensor {
 public:
  /// Starts the sensor so that ReadSample() will return data at the device's
  /// configured interval.
  pw::Status Enable() { return DoEnableLightSensor(); }

  /// Stops the sensor. ReadSample() will no longer return new samples.
  pw::Status Disable() { return DoDisableLightSensor(); }

  /// Reads an ambient light sample in lux.
  virtual pw::Result<float> ReadSampleLux() { return DoReadLightSampleLux(); }

 protected:
  // Prohibit polymorphic destruction for now.
  ~AmbientLightSensor() = default;

 private:
  virtual pw::Status DoEnableLightSensor() = 0;
  virtual pw::Status DoDisableLightSensor() = 0;
  virtual pw::Result<float> DoReadLightSampleLux() = 0;
};

}  // namespace sense

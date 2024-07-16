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

#include "modules/pwm/digital_out.h"

namespace am {

/// This class represents an fake output being driven by the PWM block.
///
/// On host, there is no PWM block, so this portable alternative logs
/// has methods that logs their parameters.
class PwmDigitalOutFake : public PwmDigitalOut {
 public:
  PwmDigitalOutFake() = default;

 private:
  /// copydoc `PwmDigitalOut::Enable`.
  void DoEnable() override;

  /// copydoc `PwmDigitalOut::Disable`.
  void DoDisable() override;

  /// copydoc `PwmDigitalOut::SetLevel`.
  void DoSetLevel(uint16_t level) override;

  /// copydoc `PwmDigitalOut::SetCallback`.
  void DoSetCallback(const Callback& callback,
                     uint16_t per_interval,
                     pw::chrono::SystemClock::duration interval) override;

  /// copydoc `PwmDigitalOut::ClearCallback`.
  void DoClearCallback() override;
};

}  // namespace am

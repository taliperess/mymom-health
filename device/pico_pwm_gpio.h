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
#include <cstdint>

#include "hardware/pwm.h"
#include "modules/pwm/digital_out.h"
#include "pw_chrono/system_clock.h"
#include "pw_digital_io/polarity.h"
#include "pw_digital_io_rp2040/digital_io.h"
#include "pw_function/function.h"

namespace am {

class PicoPwmGpio : public PwmDigitalOut {
 public:
  using GpioConfig = ::pw::digital_io::Rp2040Config;
  using Callback = ::pw::Function<void()>;

  PicoPwmGpio(const GpioConfig& config);

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
                     pw::chrono::SystemClock::duration interval_ms) override;

  /// copydoc `PwmDigitalOut::ClearCallback`.
  void DoClearCallback() override;

  uint16_t slice_num_;
  const GpioConfig& gpio_config_;
  pwm_config pwm_config_;
  uint16_t level_ = 0;
};

}  // namespace am

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

// SimpleLED implementation for the rp2040 using the pico-sdk.

#include "device/pwm_gpio.h"
#include "modules/led/monochrome_led.h"
#include "pw_digital_io_rp2040/digital_io.h"

namespace am {

/// Implementation of MonochromeLed that drives the Pi Pico's on-board LED.
class PicoLed : public MonochromeLed {
 public:
  PicoLed();

 private:
  /// @copydoc ``MonochromeLed::GetState``
  State GetState() override;

  /// @copydoc ``MonochromeLed::SetState``
  void SetState(State state) override;

  /// @copydoc ``MonochromeLed::SetBrightness``
  void DoSetBrightness(uint16_t level) override;

  /// @copydoc ``MonochromeLed::SetCallback``
  void SetCallback(Callback callback, uint16_t per_interval, uint32_t interval_ms) override;

  State state_ = State::kOff;
  pw::digital_io::Rp2040DigitalInOut led_;
  PwmGpio pwm_gpio_;
};

}  // namespace am

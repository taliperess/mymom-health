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

#include "device/pwm_gpio.h"

#include <algorithm>
#include <limits>

#include "hardware/irq.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pw_assert/check.h"
#include "pw_log/log.h"

namespace am {

PwmGpio::PwmGpio(uint16_t pin,
                 pw::digital_io::Polarity polarity)
    : pin_(pin), polarity_(polarity) {
  slice_num_ = pwm_gpio_to_slice_num(pin);
  config_ = pwm_get_default_config();
}

PwmGpio::~PwmGpio() { Disable(); }

void PwmGpio::SetCallback(Callback callback, uint16_t per_interval, uint32_t interval_ms) {
  constexpr float kClkDivMax = 256.f;
  float freq = 1000.f / interval_ms;
  float clkdiv = 65536.f / (60.f * freq * per_interval);
  clkdiv = std::min(clkdiv, kClkDivMax);

  PW_LOG_INFO("Pulsing at frequency of %f times per second", freq, clkdiv);
  if (clkdiv < 1.f) {
    uint16_t wrap = std::numeric_limits<uint16_t>::max();
    wrap = static_cast<uint16_t>(clkdiv * wrap);
    pwm_config_set_wrap(&config_, wrap);
  } else {
    pwm_config_set_clkdiv(&config_, clkdiv);
  }
  callback_ = callback;
}

void PwmGpio::Enable() {
  gpio_set_function(pin_, GPIO_FUNC_PWM);
  pwm_clear_irq(slice_num_);
  if (callback_ != nullptr) {
    pwm_set_irq_enabled(slice_num_, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, callback_);
    irq_set_enabled(PWM_IRQ_WRAP, true);
  }
  pwm_init(slice_num_, &config_, true);
}

void PwmGpio::Disable() {
  pwm_set_enabled(slice_num_, false);
  irq_set_enabled(PWM_IRQ_WRAP, false);
  pwm_set_irq_enabled(slice_num_, false);
  pwm_clear_irq(slice_num_);
  gpio_deinit(pin_);
  callback_ = nullptr;
}

void PwmGpio::SetLevel(uint16_t level) {
  pwm_clear_irq(slice_num_);
  if (polarity_ == pw::digital_io::Polarity::kActiveLow) {
    level = std::numeric_limits<uint16_t>::max() - level;
  }
  pwm_set_gpio_level(pin_, level);
}

}  // namespace am

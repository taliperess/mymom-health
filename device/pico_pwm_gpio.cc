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

#define PW_LOG_MODULE_NAME "PWM"

#include "device/pico_pwm_gpio.h"

#include <algorithm>
#include <chrono>
#include <limits>

#include "hardware/irq.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pw_log/log.h"

namespace sense {

PicoPwmGpio::PicoPwmGpio(const GpioConfig& config) : gpio_config_(config) {
  slice_num_ = pwm_gpio_to_slice_num(gpio_config_.pin);
  pwm_config_ = pwm_get_default_config();
}

void PicoPwmGpio::DoEnable() {
  gpio_set_function(gpio_config_.pin, GPIO_FUNC_PWM);
  pwm_clear_irq(slice_num_);
  if (gpio_with_callback == this) {
    EnablePwmIrq();
  }
  pwm_init(slice_num_, &pwm_config_, true);
  pwm_set_gpio_level(gpio_config_.pin, level_);
}

void PicoPwmGpio::DoDisable() {
  pwm_set_enabled(slice_num_, false);
  DisablePwmIrq();
  gpio_deinit(gpio_config_.pin);
}

void PicoPwmGpio::DoSetLevel(uint16_t level) {
  pwm_clear_irq(slice_num_);
  level_ = gpio_config_.polarity == pw::digital_io::Polarity::kActiveLow
               ? std::numeric_limits<uint16_t>::max() - level
               : level;
  pwm_set_gpio_level(gpio_config_.pin, level_);
}

void PicoPwmGpio::DoSetCallback(uint16_t per_interval,
                                pw::chrono::SystemClock::duration interval) {
  if (gpio_with_callback != nullptr) {
    PW_LOG_INFO("Replacing existing callback for slice %hu with slice %hu",
                gpio_with_callback->slice_num_,
                slice_num_);
    gpio_with_callback->DisablePwmIrq();
    if (gpio_with_callback != this) {
      gpio_with_callback->ClearCallbackFunction();
    }
  }
  gpio_with_callback = this;

  constexpr float kClkDivMax = 256.f;
  auto interval_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(interval).count();
  float freq = 1000.f / interval_ms;
  float clkdiv = 65536.f / (60.f * freq * per_interval);
  clkdiv = std::min(clkdiv, kClkDivMax);

  PW_LOG_INFO("Pulsing at frequency of %f times per second", freq);
  if (clkdiv < 1.f) {
    uint16_t wrap = std::numeric_limits<uint16_t>::max();
    wrap = static_cast<uint16_t>(clkdiv * wrap);
    pwm_config_set_wrap(&pwm_config_, wrap);
  } else {
    pwm_config_set_clkdiv(&pwm_config_, clkdiv);
  }

  EnablePwmIrq();
}

void PicoPwmGpio::DoClearCallback() {
  DisablePwmIrq();
  gpio_with_callback = nullptr;
}

void PicoPwmGpio::EnablePwmIrq() const {
  irq_set_exclusive_handler(PWM_IRQ_WRAP, IrqHandler);
  pwm_clear_irq(slice_num_);
  irq_set_enabled(PWM_IRQ_WRAP, true);
  pwm_set_irq_enabled(slice_num_, true);
}

void PicoPwmGpio::DisablePwmIrq() const {
  irq_set_enabled(PWM_IRQ_WRAP, false);
  pwm_set_irq_enabled(slice_num_, false);
  pwm_clear_irq(slice_num_);
}

// The PWM block triggers callbacks by raising "wrap" interrupts at a configured
// interval. At most one exclusive IRQ handler may be installed at any one time,
// so a pointer to the active PicoPwmGpio is stored as a singleton.
void PicoPwmGpio::IrqHandler() {
  if (gpio_with_callback != nullptr) {
    gpio_with_callback->InvokeCallback();
  }
}

PicoPwmGpio* PicoPwmGpio::gpio_with_callback = nullptr;

}  // namespace sense

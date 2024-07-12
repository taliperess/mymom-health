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

#include <cstdint>

#include "modules/led/monochrome_led.h"

namespace am {

/// Interface for a simple LED.
class PolychromeLed {
 public:
  using Callback = MonochromeLed::Callback;

  static constexpr uint32_t kRedShift = 16;
  static constexpr uint32_t kGreenShift = 8;
  static constexpr uint32_t kBlueShift = 0;

  virtual ~PolychromeLed() = default;

  uint32_t hex() const { return hex_; }
  uint8_t red() const { return static_cast<uint8_t>(hex_ >> kRedShift); }
  uint8_t green() const { return static_cast<uint8_t>(hex_ >> kGreenShift); }
  uint8_t blue() const { return static_cast<uint8_t>(hex_ >> kBlueShift); }

  uint32_t alternate_hex() const { return alternate_hex_; };
  void set_alternate_hex(uint32_t hex) { alternate_hex_ = hex; }

  uint16_t brightness() const { return brightness_; }

  /// Turns off the LED.
  void TurnOff() { SetEnabled(false); }

  // Turns the LED on.
  void TurnOn()  { SetEnabled(true); }

  // TODO
  void SetBrightness(uint8_t level);

  // TODO
  void SetColor(uint8_t red, uint8_t green, uint8_t blue);

  // TODO
  void SetColor(uint32_t hex);

  // TODO
  void Pulse(uint32_t hex, uint32_t interval_ms);

  void PulseBetween(uint32_t hex1, uint32_t hex2, uint32_t interval_ms);

  void Rainbow(uint32_t interval_ms);

 protected:
  PolychromeLed();

  // TODO
  virtual void SetEnabled(bool enable) = 0;

  // TODO
  virtual void Update() = 0;

  // TODO
  virtual void SetCallback(Callback callback, uint16_t per_interval, uint32_t interval_ms) = 0;

  uint32_t hex_ = 0;
  uint32_t alternate_hex_ = 0;
  uint16_t brightness_ = 0;
};



}  // namespace am

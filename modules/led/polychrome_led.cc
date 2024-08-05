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

#define PW_LOG_MODULE_NAME "LED"
#define PW_LOG_LEVEL PW_LOG_LEVEL_INFO

#include "modules/led/polychrome_led.h"

#include "pw_assert/check.h"
#include "pw_log/log.h"

namespace sense {

void PolychromeLed::Enable() {
  state_ = kOff;
  red_.Enable();
  green_.Enable();
  blue_.Enable();
  UpdateZeroBrightness();  // Start the LED off.
}

void PolychromeLed::Disable() {
  state_ = kDisabled;
  UpdateZeroBrightness();
  red_.Disable();
  green_.Disable();
  blue_.Disable();
}

void PolychromeLed::TurnOff() {
  if (state_ == kOn) {
    UpdateZeroBrightness();
    state_ = kOff;
    PW_LOG_DEBUG("LED off");
  }
}

void PolychromeLed::TurnOn() {
  PW_DCHECK_INT_NE(
      state_, kDisabled, "Cannot turn on the LED until Enable() is called");
  if (state_ == kOff) {
    Update();
    state_ = kOn;
    PW_LOG_DEBUG("LED on");
  }
}

void PolychromeLed::SetBrightness(uint8_t brightness) {
  red_.ClearCallback();
  if (brightness_ == brightness) {
    return;
  }

  brightness_ = brightness;
  if (state_ == kOn) {
    Update();
  }
}

void PolychromeLed::SetColor(uint32_t color_hex) {
  red_.ClearCallback();
  if (color_ == color_hex) {
    return;
  }

  color_ = color_hex;
  if (state_ == kOn) {
    Update();
  }
}

void PolychromeLed::Pulse(uint32_t color_hex, uint32_t interval_ms) {
  TurnOff();
  brightness_ = 0;
  color_ = color_hex;
  red_.SetCallback(
      [this]() {
        static uint16_t counter = 0;
        if (counter < 0x100) {
          brightness_ = counter;
        } else {
          brightness_ = 0x200 - counter;
        }
        Update();
        counter = (counter + 1) % 0x200;
      },
      0x200,
      interval_ms);
  TurnOn();
}

void PolychromeLed::Rainbow(uint32_t interval_ms) {
  TurnOff();
  brightness_ = 0xff;
  color_ = 0xff0000;
  red_.SetCallback(
      [this]() {
        static uint16_t counter = 0;
        if (counter < 0x100) {
          color_ = 0xff0000 + (counter << 8);
        } else if (counter < 0x200) {
          color_ = 0xffff00 - ((counter - 0x100) << 16);
        } else if (counter < 0x300) {
          color_ = 0x00ff00 + (counter - 0x200);
        } else if (counter < 0x400) {
          color_ = 0x00ffff - ((counter - 0x300) << 8);
        } else if (counter < 0x500) {
          color_ = 0x0000ff + ((counter - 0x400) << 16);
        } else {
          color_ = 0xff00ff - (counter - 0x500);
        }
        Update();
        counter = (counter + 1) % 0x600;
      },
      0x600,
      interval_ms);
  TurnOn();
}

void PolychromeLed::Update() {
  PW_LOG_DEBUG("LED update: rgb=%06x brightness=%hu", color_, brightness_);
  red_.SetLevel(GammaCorrect(color_ >> kRedShift));
  green_.SetLevel(GammaCorrect(color_ >> kGreenShift));
  blue_.SetLevel(GammaCorrect(color_ >> kBlueShift));
}

void PolychromeLed::UpdateZeroBrightness() {
  PW_LOG_DEBUG("LED update: rgb=%06x brightness=0", color_);
  red_.SetLevel(0);
  green_.SetLevel(0);
  blue_.SetLevel(0);
}

uint16_t PolychromeLed::GammaCorrect(uint32_t val) const {
  /// sRGB gamma correction is given by g(x) = ((x/255)^2.2)*255, rounded down.
  // clang-format off
  static constexpr std::array<uint8_t, 256> kGammaCorrection = {
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   1,   1,
    1,   1,   1,   1,   1,   2,   2,   2,
    2,   2,   2,   3,   3,   3,   3,   3,
    4,   4,   4,   4,   5,   5,   5,   5,
    6,   6,   6,   7,   7,   7,   8,   8,
    8,   9,   9,   9,   10,  10,  11,  11,
    11,  12,  12,  13,  13,  13,  14,  14,
    15,  15,  16,  16,  17,  17,  18,  18,
    19,  19,  20,  21,  21,  22,  22,  23,
    23,  24,  25,  25,  26,  27,  27,  28,
    29,  29,  30,  31,  31,  32,  33,  34,
    34,  35,  36,  37,  37,  38,  39,  40,
    40,  41,  42,  43,  44,  45,  46,  46,
    47,  48,  49,  50,  51,  52,  53,  54,
    55,  56,  57,  58,  59,  60,  61,  62,
    63,  64,  65,  66,  67,  68,  69,  70,
    71,  72,  73,  74,  76,  77,  78,  79,
    80,  81,  83,  84,  85,  86,  88,  89,
    90,  91,  93,  94,  95,  96,  98,  99,
    100, 102, 103, 104, 106, 107, 109, 110,
    111, 113, 114, 116, 117, 119, 120, 121,
    123, 124, 126, 128, 129, 131, 132, 134,
    135, 137, 138, 140, 142, 143, 145, 146,
    148, 150, 151, 153, 155, 157, 158, 160,
    162, 163, 165, 167, 169, 170, 172, 174,
    176, 178, 179, 181, 183, 185, 187, 189,
    191, 193, 194, 196, 198, 200, 202, 204,
    206, 208, 210, 212, 214, 216, 218, 220,
    222, 224, 227, 229, 231, 233, 235, 237,
    239, 241, 244, 246, 248, 250, 252, 255,
  };
  // clang-format on
  return static_cast<uint16_t>(kGammaCorrection[val % 256]) * brightness_;
}

}  // namespace sense

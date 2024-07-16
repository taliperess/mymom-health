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

#include "modules/led/polychrome_led.h"

namespace am {

void PolychromeLed::TurnOff() {
  red_.Disable();
  green_.Disable();
  blue_.Disable();
}

void PolychromeLed::TurnOn() {
  red_.Enable();
  green_.Enable();
  blue_.Enable();
  Update();
}

void PolychromeLed::SetBrightness(uint8_t brightness) {
  TurnOff();
  red_.ClearCallback();
  brightness_ = brightness;
  TurnOn();
}

void PolychromeLed::SetColor(uint8_t red, uint8_t green, uint8_t blue) {
  TurnOff();
  red_.ClearCallback();
  hex_ = 0;
  hex_ |= uint32_t(red) << kRedShift;
  hex_ |= uint32_t(green) << kGreenShift;
  hex_ |= uint32_t(blue) << kBlueShift;
  TurnOn();
}

void PolychromeLed::SetColor(uint32_t hex) {
  TurnOff();
  red_.ClearCallback();
  hex_ = hex;
  TurnOn();
}

void PolychromeLed::Pulse(uint32_t hex, uint32_t interval_ms) {
  TurnOff();
  brightness_ = 0;
  hex_ = hex;
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

void PolychromeLed::PulseBetween(uint32_t hex1,
                                 uint32_t hex2,
                                 uint32_t interval_ms) {
  TurnOff();
  brightness_ = 0;
  hex_ = hex1;
  alternate_hex_ = hex2;
  red_.SetCallback(
      [this]() {
        static uint16_t counter = 0;
        if (counter < 0x100) {
          brightness_ = static_cast<uint8_t>(counter);
        } else if (counter < 0x200) {
          brightness_ = static_cast<uint8_t>(0x200 - counter);
        }
        ++counter;
        if (counter == 0x200) {
          uint32_t hex = hex_;
          hex_ = alternate_hex_;
          alternate_hex_ = hex;
        }
        Update();
        counter %= 0x200;
      },
      0x200,
      interval_ms);
  TurnOn();
}

void PolychromeLed::Rainbow(uint32_t interval_ms) {
  TurnOff();
  brightness_ = 0xff;
  hex_ = 0xff0000;
  red_.SetCallback(
      [this]() {
        static uint16_t counter = 0;
        if (counter < 0x100) {
          hex_ = 0xff0000 + (counter << 8);
        } else if (counter < 0x200) {
          hex_ = 0xffff00 - ((counter - 0x100) << 16);
        } else if (counter < 0x300) {
          hex_ = 0x00ff00 + (counter - 0x200);
        } else if (counter < 0x400) {
          hex_ = 0x00ffff - ((counter - 0x300) << 8);
        } else if (counter < 0x500) {
          hex_ = 0x0000ff + ((counter - 0x400) << 16);
        } else {
          hex_ = 0xff00ff - (counter - 0x500);
        }
        Update();
        counter = (counter + 1) % 0x600;
      },
      0x600,
      interval_ms);
  TurnOn();
}

void PolychromeLed::Update() {
  red_.SetLevel(GammaCorrect(hex_ >> kRedShift));
  green_.SetLevel(GammaCorrect(hex_ >> kGreenShift));
  blue_.SetLevel(GammaCorrect(hex_ >> kBlueShift));
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
  uint16_t u16 = uint16_t(kGammaCorrection[val % 256]) * brightness_;
  return u16;
}

}  // namespace am

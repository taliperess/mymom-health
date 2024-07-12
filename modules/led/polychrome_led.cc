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

// TODO
PolychromeLed* singleton = nullptr;

PolychromeLed::PolychromeLed() {
  singleton = this;
}

// TODO
void PolychromeLed::SetBrightness(uint8_t brightness) {
  brightness_ = brightness;
  Update();
}

// TODO
void PolychromeLed::SetColor(uint8_t red, uint8_t green, uint8_t blue) {
  hex_ = 0;
  hex_ |= uint32_t(red) << kRedShift;
  hex_ |= uint32_t(green) << kGreenShift;
  hex_ |= uint32_t(blue) << kBlueShift;
  Update();
}

// TODO
void PolychromeLed::SetColor(uint32_t hex) {
  hex_ = hex;
  Update();
}

static void DoPulse() {
  static uint16_t counter = 0;
  if (counter < 0x100) {
    singleton->SetBrightness(counter);
  } else {
    singleton->SetBrightness(0x200 - counter);
  }
  counter = (counter + 1) % 0x200;
}

void PolychromeLed::Pulse(uint32_t hex, uint32_t interval_ms) {
  brightness_ = 0;
  hex_ = hex;
  SetCallback(DoPulse, 0x200, interval_ms);
  SetEnabled(true);
}

static void DoPulseBetween() {
  static uint16_t counter = 0;
  if (counter < 0x100) {
    singleton->SetBrightness(static_cast<uint8_t>(counter));
  } else if (counter < 0x200) {
    singleton->SetBrightness(static_cast<uint8_t>(0x200 - counter));
  }
  ++counter;
  if (counter == 0x200) {
    uint32_t hex = singleton->hex();
    singleton->SetColor(singleton->alternate_hex());
    singleton->set_alternate_hex(hex);
  }
  counter %= 0x200;
}

void PolychromeLed::PulseBetween(uint32_t hex1, uint32_t hex2, uint32_t interval_ms) {
  brightness_ = 0;
  hex_ = hex1;
  alternate_hex_ = hex2;
  SetCallback(DoPulseBetween, 0x200, interval_ms);
  SetEnabled(true);
}

static void DoRainbow() {
  static uint16_t counter = 0;
  uint32_t color = 0;
  if (counter < 0x100) {
    color = 0xff0000 + (counter << 8);
  } else if (counter < 0x200) {
    color = 0xffff00 - ((counter - 0x100) << 16);
  } else if (counter < 0x300) {
    color = 0x00ff00 + (counter - 0x200);
  } else if (counter < 0x400) {
    color = 0x00ffff - ((counter - 0x300) << 8);
  } else if (counter < 0x500) {
    color = 0x0000ff + ((counter - 0x400) << 16);
  } else {
    color = 0xff00ff - (counter - 0x500);
  }
  singleton->SetColor(color);
  counter = (counter + 1) % 0x600;
}

void PolychromeLed::Rainbow(uint32_t interval_ms) {
  SetEnabled(false);
  brightness_ = 0xff;
  hex_ = 0xff0000;
  SetCallback(DoRainbow, 0x600, interval_ms);
  SetEnabled(true);
}

}  // namespace am

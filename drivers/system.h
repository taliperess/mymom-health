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

namespace am {

/// Turns the LED on the board on or off.
///
/// @param  enable  True turns the LED on; false turns it off.
void SystemSetLed(bool enable);

/// Returns the CPU core temperature, in degress Celsius.
float SystemReadTemp();

/// Bit flags used to indicate whether to reboot using interrupt masks for
/// mass storage, picoboot, or both.
enum class RebootType : uint8_t {
  kMassStorage = 0x1,
  kPicoboot = 0x2,
};

/// Reboot the board.
///
/// @param  reboot_types  Bit-flag combination of ``RebootType``s.
void SystemReboot(uint8_t reboot_types);

}  // namespace am

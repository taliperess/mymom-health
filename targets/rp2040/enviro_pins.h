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

namespace am::board {

// Pins are declared as `unsigned int` to match the types used in the Pico SDK.
inline constexpr unsigned int kEnviroPinPmsReset = 2;
inline constexpr unsigned int kEnviroPinPmsEn = 3;
inline constexpr unsigned int kEnviroPinSda = 4;
inline constexpr unsigned int kEnviroPinScl = 5;
inline constexpr unsigned int kEnviroPinLedR = 6;
inline constexpr unsigned int kEnviroPinLedG = 7;
inline constexpr unsigned int kEnviroPmsRx = 8;
inline constexpr unsigned int kEnviroPmsTx = 9;
inline constexpr unsigned int kEnviroPinLedB = 10;
inline constexpr unsigned int kEnviroPinSwA = 12;
inline constexpr unsigned int kEnviroPinSwB = 13;
inline constexpr unsigned int kEnviroPinSwX = 14;
inline constexpr unsigned int kEnviroPinSwY = 15;
inline constexpr unsigned int kEnviroLcdDc = 16;
inline constexpr unsigned int kEnviroLcdCs = 17;
inline constexpr unsigned int kEnviroLcdSclk = 18;
inline constexpr unsigned int kEnviroLcdMosi = 19;
inline constexpr unsigned int kEnviroBacklightEn = 20;
inline constexpr unsigned int kEnviroLcdVSync = 21;
inline constexpr unsigned int kEnviroLtr550Int = 22;
inline constexpr unsigned int kEnviroMicOut = 26;

}  // namespace am::board

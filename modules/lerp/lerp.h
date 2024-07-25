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

namespace sense {
/// Linearly interpolate between a and b using the specified fraction.
///
/// Does not do any bounds checking to ensure that the fraction is between
/// 0 and 1.
///
/// @param  a          The A value for interpolation.
/// @param  b          The B value for interpolation.
/// @param numerator   The numerator of the fraction used to interpolate.
/// @param denominator The denominator of the fraction used to interpolate.

static constexpr uint8_t Lerp(uint8_t a,
                              uint8_t b,
                              uint16_t numerator,
                              uint16_t denominator) {
  // Do calculations in 32bit space to avoid overflows due to multiplication.
  // 8 bits of value (a and b) * 16 bits of numerator yields 24 bits.
  // That 24 bits divided by the denominator yields an 8 bit value to return.
  int32_t a_32 = a;
  int32_t b_32 = b;

  return static_cast<uint8_t>(a_32 + (b_32 - a_32) *
                                         static_cast<int32_t>(numerator) /
                                         static_cast<int32_t>(denominator));
}
}  // namespace sense

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

#include <array>
#include <type_traits>

namespace sense {

/// Performs integer division and rounds to the nearest integer.
template <typename T>
constexpr T IntegerDivisionRoundNearest(T dividend, T divisor) {
  static_assert(std::is_integral_v<T>);
  // Integer division truncates towards zero, so change the direction of the
  // bias when the quotient is negative.
  if constexpr (std::is_signed_v<T>) {
    if ((dividend < 0) != (divisor < 0)) {
      return (dividend - divisor / T{2}) / divisor;
    }
  }
  return (dividend + divisor / T{2}) / divisor;
}

/// Calculates the mean of the previous `kWindowSize` integer data points.
/// Returns the mean rounded to the nearest integer.
///
/// Performs no floating point operations.
template <typename T, size_t kWindowSize>
class IntegerSimpleMovingAverager {
 public:
  explicit constexpr IntegerSimpleMovingAverager(T initial_value = T())
      : window_{}, index_(0), sum_(initial_value * kWindowSize) {
    for (T& sample : window_) {
      sample = initial_value;
    }
  }

  constexpr T Average() const {
    return static_cast<T>(IntegerDivisionRoundNearest<Sum>(sum_, kWindowSize));
  }

  constexpr void Update(T sample) {
    index_ = (index_ + 1) % kWindowSize;
    sum_ -= window_[index_];  // replace the oldest sample with the new one
    sum_ += sample;
    window_[index_] = sample;
  }

 private:
  static_assert(std::is_integral_v<T>);

  // Use a larger type for the sum to avoid overflow.
  using SignedSum = std::conditional_t<sizeof(T) < sizeof(int), int, long long>;
  using Sum = std::conditional_t<std::is_signed_v<T>,
                                 SignedSum,
                                 std::make_unsigned_t<SignedSum>>;

  std::array<T, kWindowSize> window_;
  size_t index_;
  Sum sum_;
};

}  // namespace sense

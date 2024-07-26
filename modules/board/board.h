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

#include "modules/board/board.rpc.pb.h"
#include "pw_status/status.h"

namespace sense {

/// Interface for Pico board functionality not associated with another
/// component.
class Board {
 public:
  virtual ~Board() = default;

  /// Returns the CPU core temperature, in degress Celsius.
  virtual float ReadInternalTemperature() = 0;

  /// Reboot the board.
  ///
  /// Returns OK if rebooting, or INVALID_ARGUMENT if the reboot type is not
  /// recognized.
  ///
  /// @param  reboot_types  Indicates whether to disable the picoboot interface,
  /// mass storage interface, or neither (cold boot) upon reboot.
  virtual pw::Status Reboot(board_RebootType_Enum reboot_type) = 0;

  /// Returns the serial number of the board's flash.
  virtual uint64_t UniqueFlashId() const = 0;

 protected:
  Board() = default;
};

}  // namespace sense

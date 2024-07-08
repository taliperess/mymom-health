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

#include "modules/board/board.rpc.pb.h"

namespace am {

/// Implements and extends the ``Board`` interface to facilitate unit testing.
class BoardFake : public Board {
 public:
  board_RebootType_Enum last_reboot_type() const { return last_reboot_type_; }
  void set_internal_temperature(float internal_temperature) {
    internal_temperature_ = internal_temperature;
  }

  /// @copydoc ``Board::ReadInternalTemperature``.
  float ReadInternalTemperature() override { return internal_temperature_; }

  /// @copydoc ``Board::Reboot``.
  pw::Status Reboot(board_RebootType_Enum reboot_type) override {
    last_reboot_type_ = reboot_type;
    return pw::OkStatus();
  }

 private:
  float internal_temperature_ = 20.0f;
  board_RebootType_Enum last_reboot_type_ = board_RebootType_Enum_UNKNOWN;
};

}  // namespace am

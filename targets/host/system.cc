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

#include "system/system.h"

#include "modules/board/board_fake.h"
#include "modules/led/monochrome_led_fake.h"
#include "pw_assert/check.h"

namespace am::system {

void Init() {}

void Start() {
  PW_CHECK(false, "Host system startup has not been implemented.");
}

am::Board& Board() {
  static BoardFake board;
  return board;
}

am::MonochromeLed& MonochromeLed() {
  static MonochromeLedFake monochrome_led;
  return monochrome_led;
}

}  // namespace am::system

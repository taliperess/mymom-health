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

#define PW_LOG_MODULE_NAME "MAIN"

#include "modules/board/service.h"
#include "modules/color_rotation/manager.h"
#include "modules/pubsub/service.h"
#include "modules/state_manager/state_manager.h"
#include "pw_log/log.h"
#include "pw_system/system.h"
#include "system/pubsub.h"
#include "system/system.h"
#include "system/worker.h"

namespace {
const std::array kColorRotationSteps{
    am::ColorRotationManager::Step{
        .r = 0xd6, .g = 0x02, .b = 0x70, .num_cycles = 2500},
    am::ColorRotationManager::Step{
        .r = 0x9b, .g = 0x4f, .b = 0x96, .num_cycles = 2500},
    am::ColorRotationManager::Step{
        .r = 0x00, .g = 0x38, .b = 0xa8, .num_cycles = 2500},
};
}
int main() {
  am::system::Init();

  am::StateManager state_manager(am::system::PubSub(),
                                 am::system::PolychromeLed());
  state_manager.Init();

  am::ColorRotationManager color_rotation_manager(
      kColorRotationSteps, am::system::PubSub(), am::system::GetWorker());
  color_rotation_manager.Start();

  static am::BoardService board_service;
  board_service.Init(am::system::GetWorker(), am::system::Board());
  pw::System().rpc_server().RegisterService(board_service);

  static am::PubSubService pubsub_service;
  pubsub_service.Init(am::system::PubSub());
  pw::System().rpc_server().RegisterService(pubsub_service);

  auto& button_manager = am::system::ButtonManager();
  button_manager.Init(am::system::PubSub(), am::system::GetWorker());

  PW_LOG_INFO("Welcome to Airmaranth 🌿☁️");
  am::system::Start();
  PW_UNREACHABLE;
}

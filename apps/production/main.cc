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

#include "apps/production/threads.h"
#include "modules/board/service.h"
#include "modules/color_rotation/manager.h"
#include "modules/proximity/manager.h"
#include "modules/pubsub/service.h"
#include "modules/sampling_thread/sampling_thread.h"
#include "modules/state_manager/state_manager.h"
#include "pw_log/log.h"
#include "pw_system/system.h"
#include "pw_thread/detached_thread.h"
#include "system/pubsub.h"
#include "system/system.h"
#include "system/worker.h"

namespace am {
namespace {

const std::array kColorRotationSteps{
    ColorRotationManager::Step{
        .r = 0xd6, .g = 0x02, .b = 0x70, .num_cycles = 2500},
    ColorRotationManager::Step{
        .r = 0x9b, .g = 0x4f, .b = 0x96, .num_cycles = 2500},
    ColorRotationManager::Step{
        .r = 0x00, .g = 0x38, .b = 0xa8, .num_cycles = 2500},
};

void InitBoardService() {
  static StateManager state_manager(system::PubSub(), system::PolychromeLed());
  state_manager.Init();

  static ColorRotationManager color_rotation_manager(
      kColorRotationSteps, system::PubSub(), system::GetWorker());
  color_rotation_manager.Start();

  static BoardService board_service;
  board_service.Init(system::GetWorker(), system::Board());
  pw::System().rpc_server().RegisterService(board_service);
}

void InitProximitySensor() {
  constexpr uint16_t kInitialNearTheshold = 16384;
  constexpr uint16_t kInitialFarTheshold = 512;
  static ProximityManager proximity(
      system::PubSub(), kInitialFarTheshold, kInitialNearTheshold);

  system::PubSub().SubscribeTo<ProximityStateChange>(
      [](ProximityStateChange state) {
        if (state.proximity) {
          PW_LOG_INFO("Proximity detected!");
        } else {
          PW_LOG_INFO("Proximity NOT detected!");
        }
      });
}

[[noreturn]] void InitializeApp() {
  system::Init();

  InitBoardService();
  InitProximitySensor();

  pw::thread::DetachedThread(SamplingThreadOptions(), SamplingLoop);

  static PubSubService pubsub_service;
  pubsub_service.Init(system::PubSub());
  pw::System().rpc_server().RegisterService(pubsub_service);

  auto& button_manager = system::ButtonManager();
  button_manager.Init(system::PubSub(), system::GetWorker());

  PW_LOG_INFO("Welcome to Airmaranth 🌿☁️");
  system::Start();
}

}  // namespace
}  // namespace am

int main() { am::InitializeApp(); }

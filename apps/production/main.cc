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
#include "modules/air_sensor/service.h"
#include "modules/board/service.h"
#include "modules/color_rotation/manager.h"
#include "modules/morse_code/encoder.h"
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

constexpr LedValue kMorseCodeLedColor(0, 255, 255);

void InitBoardService() {
  static StateManager state_manager(system::PubSub(), system::PolychromeLed());
  state_manager.Init();

  static ColorRotationManager color_rotation_manager(
      kColorRotationSteps, system::PubSub(), system::GetWorker());
  color_rotation_manager.Start();

  // The morse encoder will emit pubsub events to the state manager.
  static Encoder morse_encoder;
  morse_encoder.Init(am::system::GetWorker(), [](bool turn_on) {
    if (turn_on) {
      system::PubSub().Publish(LedValueMorseCodeMode(kMorseCodeLedColor));
    } else {
      system::PubSub().Publish(LedValueMorseCodeMode(0, 0, 0));
    }
  });
  morse_encoder.Encode("PW",
                       /*repeat=*/0,
                       Encoder::kDefaultIntervalMs);

  static BoardService board_service;
  board_service.Init(system::GetWorker(), system::Board());
  pw::System().rpc_server().RegisterService(board_service);
}

void InitProximitySensor() {
  // Set up a proximity detector state machine.
  constexpr uint16_t kInitialNearTheshold = 16384;
  constexpr uint16_t kInitialFarTheshold = 512;
  static ProximityManager proximity(
      system::PubSub(), kInitialFarTheshold, kInitialNearTheshold);

  // Log when proximity is detected.
  system::PubSub().SubscribeTo<ProximityStateChange>(
      [](ProximityStateChange state) {
        if (state.proximity) {
          PW_LOG_INFO("Proximity detected!");
        } else {
          PW_LOG_INFO("Proximity NOT detected!");
        }
      });

  // Publish LED values based on proximity samples.
  system::PubSub().SubscribeTo<ProximitySample>([](ProximitySample event) {
    const uint8_t value = static_cast<uint8_t>(event.sample >> 8);
    system::PubSub().Publish(LedValueProximityMode(value, value, value));
  });
}

void InitAirSensor() {
  static AirSensor& air_sensor = am::system::AirSensor();
  static am::AirSensorService air_sensor_service;
  air_sensor_service.Init(air_sensor);
  pw::System().rpc_server().RegisterService(air_sensor_service);
}

[[noreturn]] void InitializeApp() {
  system::Init();

  InitBoardService();
  InitProximitySensor();
  InitAirSensor();

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

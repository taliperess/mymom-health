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
#include "modules/event_timers/event_timers.h"
#include "modules/morse_code/encoder.h"
#include "modules/proximity/manager.h"
#include "modules/pubsub/service.h"
#include "modules/sampling_thread/sampling_thread.h"
#include "modules/state_manager/service.h"
#include "modules/state_manager/state_manager.h"
#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_system/system.h"
#include "pw_thread/detached_thread.h"
#include "system/pubsub.h"
#include "system/system.h"
#include "system/worker.h"

namespace sense {
namespace {

void InitStateManager() {
  static StateManager state_manager(system::PubSub(), system::PolychromeLed());
  static StateManagerService state_manager_service(system::PubSub());
  pw::System().rpc_server().RegisterService(state_manager_service);
}

void InitEventTimers() {
  auto& pubsub = system::PubSub();
  static EventTimers<3> event_timers(pubsub);
  PW_CHECK_OK(event_timers.AddEventTimer(StateManager::kRepeatAlarmToken));
  PW_CHECK_OK(event_timers.AddEventTimer(StateManager::kSilenceAlarmToken));
  PW_CHECK_OK(event_timers.AddEventTimer(StateManager::kThresholdModeToken));
}

void InitBoardService() {
  static BoardService board_service;
  board_service.Init(system::GetWorker(), system::Board());
  pw::System().rpc_server().RegisterService(board_service);
}

void InitMorseEncoder() {
  // The morse encoder will emit pubsub events to the state manager.
  static Encoder morse_encoder;
  morse_encoder.Init(system::GetWorker(),
                     [](bool turn_on, const Encoder::State& state) {
                       std::ignore = system::PubSub().Publish(MorseCodeValue{
                           .turn_on = turn_on,
                           .message_finished = state.message_finished(),
                       });
                     });

  PW_CHECK(system::PubSub().SubscribeTo<MorseEncodeRequest>(
      [](MorseEncodeRequest request) {
        PW_CHECK_OK(morse_encoder.Encode(
            request.message, request.repeat, Encoder::kDefaultIntervalMs));
      }));
}

void InitProximitySensor() {
  // Set up a proximity detector state machine.
  constexpr uint16_t kInitialNearTheshold = 16384;
  constexpr uint16_t kInitialFarTheshold = 512;
  static ProximityManager proximity(
      system::PubSub(), kInitialFarTheshold, kInitialNearTheshold);

  // Log when proximity is detected.
  PW_CHECK(system::PubSub().SubscribeTo<ProximityStateChange>(
      [](ProximityStateChange state) {
        if (state.proximity) {
          PW_LOG_INFO("Proximity detected!");
        } else {
          PW_LOG_INFO("Proximity NOT detected!");
        }
      }));
}

void InitAirSensor() {
  static AirSensor& air_sensor = sense::system::AirSensor();
  static sense::AirSensorService air_sensor_service;
  air_sensor_service.Init(system::GetWorker(), air_sensor);
  pw::System().rpc_server().RegisterService(air_sensor_service);
}

[[noreturn]] void InitializeApp() {
  system::Init();

  InitStateManager();
  InitEventTimers();
  InitBoardService();
  InitMorseEncoder();
  InitProximitySensor();
  InitAirSensor();

  pw::thread::DetachedThread(SamplingThreadOptions(), SamplingLoop);

  static PubSubService pubsub_service;
  pubsub_service.Init(system::PubSub());
  pw::System().rpc_server().RegisterService(pubsub_service);

  auto& button_manager = system::ButtonManager();
  button_manager.Init(system::PubSub(), system::GetWorker());

  PW_LOG_INFO("Welcome to Pigweed Sense 🌿☁️");
  system::Start();
}

}  // namespace
}  // namespace sense

int main() { sense::InitializeApp(); }

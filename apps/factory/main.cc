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

#include "apps/factory/service.h"
#include "modules/air_sensor/service.h"
#include "modules/blinky/service.h"
#include "modules/board/service.h"
#include "modules/proximity/manager.h"
#include "modules/pubsub/service.h"
#include "pw_log/log.h"
#include "pw_system/system.h"
#include "system/pubsub.h"
#include "system/system.h"
#include "system/worker.h"

namespace sense {
namespace {

[[noreturn]] void InitializeApp() {
  system::Init();

  static BoardService board_service;
  board_service.Init(system::GetWorker(), system::Board());
  pw::System().rpc_server().RegisterService(board_service);

  static PubSubService pubsub_service;
  pubsub_service.Init(system::PubSub());
  pw::System().rpc_server().RegisterService(pubsub_service);

  static sense::BlinkyService blinky_service;
  blinky_service.Init(
      system::GetWorker(), system::MonochromeLed(), system::PolychromeLed());
  pw::System().rpc_server().RegisterService(blinky_service);

  static AirSensor& air_sensor = system::AirSensor();
  static AirSensorService air_sensor_service;
  air_sensor_service.Init(system::GetWorker(), air_sensor);
  pw::System().rpc_server().RegisterService(air_sensor_service);

  auto& button_manager = system::ButtonManager();
  button_manager.Init(system::PubSub(), system::GetWorker());
  button_manager.Stop();

  FactoryService factory_service;
  factory_service.Init(system::Board(),
                       button_manager,
                       system::ProximitySensor(),
                       system::AmbientLightSensor());
  pw::System().rpc_server().RegisterService(factory_service);

  PW_LOG_INFO("Enviro+ Pack Diagnostics app");
  system::Start();
}

}  // namespace
}  // namespace sense

int main() { sense::InitializeApp(); }

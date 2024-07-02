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

#include "modules/blinky/blinky.h"
#include "modules/blinky/service.h"
#include "modules/indicators/system_led.h"
#include "pw_log/log.h"
#include "pw_system/rpc_server.h"
#include "pw_system/work_queue.h"

namespace pw::system {

// This will run once after pw::system::Init() completes. This callback must
// return or it will block the work queue.
void UserAppInit() {
  static am::SystemLed led;
  static am::Blinky blinky(pw::system::GetWorkQueue(), led);
  static am::BlinkyService blinky_service(blinky);
  pw::system::GetRpcServer().RegisterService(blinky_service);
  PW_LOG_INFO("Started blinky app; waiting for RPCs...");
}

} // namespace pw::system

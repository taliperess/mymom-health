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

#include <signal.h>
#include <stdio.h>

#include "modules/air_sensor/air_sensor_fake.h"
#include "modules/board/board_fake.h"
#include "modules/light/fake_sensor.h"
#include "modules/proximity/fake_sensor.h"
#include "pw_assert/check.h"
#include "pw_digital_io/digital_io.h"
#include "pw_multibuf/simple_allocator.h"
#include "pw_system/io.h"
#include "pw_system/system.h"
#include "targets/host/stream_channel.h"

using pw::digital_io::DigitalIn;
using pw::digital_io::State;

extern "C" {

void CtrlCSignalHandler(int /* ignored */) {
  printf("\nCtrl-C received; simulator exiting immediately...\n");
  exit(0);
}

}  // extern "C"

void InstallCtrlCSignalHandler() {
  // Catch Ctrl-C to force a 0 exit code (success) to avoid signaling an error
  // for intentional exits. For example, VSCode shows an alarming dialog on
  // non-zero exit, which is confusing for users intentionally quitting.
  signal(SIGINT, CtrlCSignalHandler);
}

namespace {
class VirtualInput : public DigitalIn {
 public:
  VirtualInput(State state) : state_(state) {}

 private:
  pw::Status DoEnable(bool) override { return pw::OkStatus(); }
  pw::Result<State> DoGetState() override { return state_; }

  State state_;
};

VirtualInput io_sw_a(State::kInactive);
VirtualInput io_sw_b(State::kInactive);
VirtualInput io_sw_x(State::kInactive);
VirtualInput io_sw_y(State::kInactive);

}  // namespace

namespace sense::system {

void Init() {}

void Start() {
  InstallCtrlCSignalHandler();
  printf("=====================================\n");
  printf("=== Pigweed Sense: Host Simulator ===\n");
  printf("=====================================\n");
  printf("Simulator is now running. To connect with a console,\n");
  printf("either run one in a new terminal:\n");
  printf("\n");
  printf("   $ bazelisk run //<app>:simulator_console\n");
  printf("\n");
  printf("where <app> is e.g. blinky, factory, or production, or launch\n");
  printf("one from VSCode under the 'Bazel Build Targets' explorer tab.\n");
  printf("\n");
  printf("Press Ctrl-C to exit\n");

  static std::byte channel_buffer[16384];
  static pw::multibuf::SimpleAllocator multibuf_alloc(channel_buffer,
                                                      pw::System().allocator());
  static StreamChannel channel(
      multibuf_alloc, pw::system::GetReader(), pw::system::GetWriter());

  pw::SystemStart(channel);
  PW_UNREACHABLE;
}

sense::AirSensor& AirSensor() {
  static AirSensorFake air_sensor;
  return air_sensor;
}

sense::Board& Board() {
  static BoardFake board;
  return board;
}

sense::ButtonManager& ButtonManager() {
  static ::sense::ButtonManager manager(io_sw_a, io_sw_b, io_sw_x, io_sw_y);
  return manager;
}

sense::AmbientLightSensor& AmbientLightSensor() {
  static ::sense::FakeAmbientLightSensor fake_light;
  return fake_light;
}

sense::ProximitySensor& ProximitySensor() {
  static ::sense::FakeProximitySensor fake_prox;
  return fake_prox;
}

}  // namespace sense::system

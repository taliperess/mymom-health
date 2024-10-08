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

#include "device/bme688.h"
#include "device/ltr559_light_and_prox_sensor.h"
#include "device/pico_board.h"
#include "hardware/adc.h"
#include "hardware/exception.h"
#include "modules/air_sensor/air_sensor.h"
#include "modules/buttons/manager.h"
#include "pico/stdlib.h"
#include "pw_channel/rp2_stdio_channel.h"
#include "pw_cpu_exception/entry.h"
#include "pw_digital_io_rp2040/digital_io.h"
#include "pw_i2c_rp2040/initiator.h"
#include "pw_multibuf/simple_allocator.h"
#include "pw_system/system.h"
#if defined(PICO_RP2040) && PICO_RP2040
#include "system_RP2040.h"
#endif  // defined(PICO_RP2040) && PICO_RP2040
#if defined(PICO_RP2350) && PICO_RP2350
#include "system_RP2350.h"
#endif  // defined(PICO_RP2350) && PICO_RP2350
#include "system/pubsub.h"
#include "system/worker.h"
#include "targets/rp2/enviro_pins.h"

using pw::digital_io::Rp2040DigitalIn;

namespace sense::system {
namespace {

pw::i2c::Initiator& I2cInitiator() {
  static constexpr pw::i2c::Rp2040Initiator::Config kI2c0Config{
      .clock_frequency = 400'000,
      .sda_pin = board::kEnviroPinSda,
      .scl_pin = board::kEnviroPinScl,
  };

  static pw::i2c::Initiator& i2c0_bus = []() -> pw::i2c::Initiator& {
    static pw::i2c::Rp2040Initiator bus(kI2c0Config, i2c0);
    bus.Enable();
    return bus;
  }();

  return i2c0_bus;
}

Ltr559ProxAndLightSensorImpl& Ltr559() {
  static Ltr559ProxAndLightSensorImpl sensor(I2cInitiator());
  return sensor;
}

}  // namespace

namespace {
Rp2040DigitalIn io_sw_a({
    .pin = board::kEnviroPinSwA,
    .polarity = pw::digital_io::Polarity::kActiveLow,
    .enable_pull_up = true,
});

Rp2040DigitalIn io_sw_b({
    .pin = board::kEnviroPinSwB,
    .polarity = pw::digital_io::Polarity::kActiveLow,
    .enable_pull_up = true,
});

Rp2040DigitalIn io_sw_x({
    .pin = board::kEnviroPinSwX,
    .polarity = pw::digital_io::Polarity::kActiveLow,
    .enable_pull_up = true,
});

Rp2040DigitalIn io_sw_y({
    .pin = board::kEnviroPinSwY,
    .polarity = pw::digital_io::Polarity::kActiveLow,
    .enable_pull_up = true,
});
}  // namespace

void Init() {
  // PICO_SDK inits.
  SystemInit();
  stdio_init_all();
  setup_default_uart();
  stdio_usb_init();
  adc_init();

  // Install the CPU exception handler.
  exception_set_exclusive_handler(HARDFAULT_EXCEPTION, pw_cpu_exception_Entry);
}

void Start() {
  static std::byte channel_buffer[2048];
  static pw::multibuf::SimpleAllocator multibuf_alloc(channel_buffer,
                                                      pw::System().allocator());
  pw::SystemStart(pw::channel::Rp2StdioChannelInit(multibuf_alloc));
  PW_UNREACHABLE;
}

sense::AirSensor& AirSensor() {
  static Bme688 air_sensor(I2cInitiator(), sense::system::GetWorker());
  return air_sensor;
}

sense::Board& Board() {
  static ::sense::PicoBoard board;
  return board;
}

sense::ButtonManager& ButtonManager() {
  static ::sense::ButtonManager manager(io_sw_a, io_sw_b, io_sw_x, io_sw_y);
  return manager;
}

sense::AmbientLightSensor& AmbientLightSensor() { return Ltr559(); }

sense::ProximitySensor& ProximitySensor() { return Ltr559(); }

}  // namespace sense::system

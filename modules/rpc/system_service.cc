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

#include "modules/rpc/system_service.h"

#include "hardware/adc.h"
#include "pico/bootrom.h"
#include "pw_status/status.h"

namespace am::rpc {

void SystemService::Init() { adc_init(); }

pw::Status SystemService::Reboot(const am_rpc_RebootRequest& request,
                                 pw_protobuf_Empty& /*response*/) {
  uint32_t disable_interface_mask = 0;

  if (request.reboot_type ==
      am_rpc_RebootType_Enum_BOTH_MASS_STORAGE_AND_PICOBOOT) {
    disable_interface_mask = 0;
  } else if (request.reboot_type == am_rpc_RebootType_Enum_PICOBOOT_ONLY) {
    disable_interface_mask = 1;
  } else if (request.reboot_type == am_rpc_RebootType_Enum_MASS_STORAGE_ONLY) {
    disable_interface_mask = 2;
  } else {
    return pw::Status::Unknown();
  }

  reset_usb_boot(0, disable_interface_mask);
  return pw::OkStatus();
}

pw::Status SystemService::OnboardTemp(const pw_protobuf_Empty& /*request*/,
                                      am_rpc_OnboardTempResponse& response) {
  adc_set_temp_sensor_enabled(true);
  adc_select_input(4);  // 4 is the on board temp sensor.

  // See raspberry-pi-pico-c-sdk.pdf, Section '4.1.1. hardware_adc'
  const float conversion_factor = 3.3f / (1 << 12);
  float adc = static_cast<float>(adc_read()) * conversion_factor;
  float temp_c = 27.0f - (adc - 0.706f) / 0.001721f;
  response.temp = temp_c;

  return pw::OkStatus();
}

}  // namespace am::rpc

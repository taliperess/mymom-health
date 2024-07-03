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

#include "drivers/system.h"
#include "pico/bootrom.h"
#include "pw_log/log.h"
#include "pw_status/status.h"

namespace am::rpc {

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
  response.temp = SystemReadTemp();
  return pw::OkStatus();
}

void SystemService::OnboardTempStream(
    const am_rpc_OnboardTempStreamRequest& request,
    ServerWriter<am_rpc_OnboardTempResponse>& writer) {
  if (request.sample_interval_ms < 100) {
    writer.Finish(pw::Status::InvalidArgument());
    return;
  }

  temp_sample_interval_ = pw::chrono::SystemClock::for_at_least(
      std::chrono::milliseconds(request.sample_interval_ms));
  temp_sample_writer_ = std::move(writer);

  ScheduleTempSample();
}

void SystemService::TempSampleCallback() {
  float temp = SystemReadTemp();

  pw::Status status = temp_sample_writer_.Write({.temp = temp});
  if (status.ok()) {
    ScheduleTempSample();
  } else {
    PW_LOG_INFO("Temperature stream closed; ending periodic sampling");
  }
}

}  // namespace am::rpc

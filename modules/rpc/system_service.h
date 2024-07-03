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
#pragma once

#include "modules/rpc/system.rpc.pb.h"
#include "pw_chrono/system_timer.h"
#include "pw_status/status.h"

namespace am::rpc {

class SystemService final
    : public pw_rpc::nanopb::System::Service<SystemService> {
 public:
  SystemService()
      : temp_sample_timer_([this](pw::chrono::SystemClock::time_point) {
          TempSampleCallback();
        }) {}

  pw::Status Reboot(const am_rpc_RebootRequest& request,
                    pw_protobuf_Empty& /*response*/);

  pw::Status OnboardTemp(const pw_protobuf_Empty& /*request*/,
                         am_rpc_OnboardTempResponse& response);

  void OnboardTempStream(const am_rpc_OnboardTempStreamRequest& request,
                         ServerWriter<am_rpc_OnboardTempResponse>& writer);

 private:
  void TempSampleCallback();

  void ScheduleTempSample() {
    temp_sample_timer_.InvokeAfter(temp_sample_interval_);
  }

  pw::chrono::SystemTimer temp_sample_timer_;
  pw::chrono::SystemClock::duration temp_sample_interval_;
  ServerWriter<am_rpc_OnboardTempResponse> temp_sample_writer_;
};

}  // namespace am::rpc

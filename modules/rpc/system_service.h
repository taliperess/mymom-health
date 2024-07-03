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
#include "pw_status/status.h"

namespace am::rpc {

class SystemService final
    : public pw_rpc::nanopb::System::Service<SystemService> {
 public:
  void Init();

  pw::Status Reboot(const am_rpc_RebootRequest& request,
                    pw_protobuf_Empty& /*response*/);

  pw::Status OnboardTemp(const pw_protobuf_Empty& /*request*/,
                         am_rpc_OnboardTempResponse& response);
};

}  // namespace am::rpc

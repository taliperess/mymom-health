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

#include "modules/morse_code/encoder.h"
#include "modules/morse_code/morse_code.rpc.pb.h"
#include "pw_rpc/server.h"
#include "pw_status/status.h"

namespace sense {

class MorseCodeService final
    : public ::morse_code::pw_rpc::nanopb::MorseCode::Service<
          MorseCodeService> {
 public:
  static constexpr uint32_t kDefaultDitInterval = 10;

  void Init(Worker& worker, Encoder::OutputFunction&& output);

  pw::Status Send(const morse_code_SendRequest& request,
                  pw_protobuf_Empty& response);

 private:
  Encoder encoder_;
};

}  // namespace sense

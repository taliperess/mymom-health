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

#include "modules/morse_code/service.h"

namespace sense {

void MorseCodeService::Init(Worker& worker, Encoder::OutputFunction&& output) {
  encoder_.Init(worker, std::move(output));
}

pw::Status MorseCodeService::Send(const morse_code_SendRequest& request,
                                  pw_protobuf_Empty&) {
  msg_ = request.msg;
  uint32_t repeat = request.has_repeat ? request.repeat : 1;
  uint32_t interval_ms = request.has_interval_ms ? request.interval_ms
                                                 : Encoder::kDefaultIntervalMs;
  return encoder_.Encode(request.msg, repeat, interval_ms);
}

}  // namespace sense

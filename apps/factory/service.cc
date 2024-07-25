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

#include "apps/factory/service.h"

namespace sense {

void FactoryService::Init(ButtonManager& button_manager) {
  button_manager_ = &button_manager;
}

pw::Status FactoryService::StartTest(const factory_StartTestRequest& request,
                                     pw_protobuf_Empty&) {
  switch (request.test) {
    case factory_Test_Type_BUTTONS:
      button_manager_->Start();
      break;
  }

  return pw::OkStatus();
}

pw::Status FactoryService::EndTest(const factory_EndTestRequest& request,
                                   pw_protobuf_Empty&) {
  switch (request.test) {
    case factory_Test_Type_BUTTONS:
      button_manager_->Stop();
      break;
  }

  return pw::OkStatus();
}

}  // namespace sense

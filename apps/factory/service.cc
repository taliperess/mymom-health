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

#define PW_LOG_MODULE_NAME "FACT"

#include "pw_log/log.h"
#include "pw_status/try.h"

namespace sense {

void FactoryService::Init(Board& board,
                          PubSub& pubsub,
                          ButtonManager& button_manager,
                          ProximitySensor& proximity_sensor,
                          AmbientLightSensor& ambient_light_sensor,
                          AirSensor& air_sensor) {
  board_ = &board;
  pubsub_ = &pubsub;
  button_manager_ = &button_manager;
  proximity_sensor_ = &proximity_sensor;
  ambient_light_sensor_ = &ambient_light_sensor;
  air_sensor_ = &air_sensor;
}

pw::Status FactoryService::GetDeviceInfo(const pw_protobuf_Empty&,
                                         factory_DeviceInfo& response) {
  response.flash_id = board_->UniqueFlashId();
  return pw::OkStatus();
}

pw::Status FactoryService::StartTest(const factory_StartTestRequest& request,
                                     pw_protobuf_Empty&) {
  switch (request.test) {
    case factory_Test_Type_BUTTONS:
      PW_LOG_INFO("Configured for buttons test");
      button_manager_->Start();
      break;
    case factory_Test_Type_LTR559_PROX:
      PW_LOG_INFO("Configured for LTR559 proximity test");
      return proximity_sensor_->Enable();
    case factory_Test_Type_LTR559_LIGHT:
      PW_LOG_INFO("Configured for LTR559 ambient light test");
      return ambient_light_sensor_->Enable();
    case factory_Test_Type_BME688:
      PW_LOG_INFO("Configured for BME688 air sensor test");
      return air_sensor_->Init();
  }

  return pw::OkStatus();
}

pw::Status FactoryService::EndTest(const factory_EndTestRequest& request,
                                   pw_protobuf_Empty&) {
  switch (request.test) {
    case factory_Test_Type_BUTTONS:
      button_manager_->Stop();
      break;
    case factory_Test_Type_LTR559_PROX:
      return proximity_sensor_->Disable();
    case factory_Test_Type_LTR559_LIGHT:
      return ambient_light_sensor_->Disable();
    case factory_Test_Type_BME688:
      // Do nothing.
      break;
  }

  return pw::OkStatus();
}

pw::Status FactoryService::SampleLtr559Prox(
    const pw_protobuf_Empty&, factory_Ltr559ProxSample& response) {
  pw::Result<uint16_t> result = proximity_sensor_->ReadSample();
  PW_TRY(result);

  response.value = result.value();
  return pw::OkStatus();
}

pw::Status FactoryService::SampleLtr559Light(
    const pw_protobuf_Empty&, factory_Ltr559LightSample& response) {
  pw::Result<float> result = ambient_light_sensor_->ReadSampleLux();
  PW_TRY(result);

  response.lux = result.value();
  return pw::OkStatus();
}

}  // namespace sense

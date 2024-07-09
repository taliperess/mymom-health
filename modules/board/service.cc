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

#include "modules/board/service.h"

#include "modules/board/board.h"
#include "pw_log/log.h"
#include "pw_status/status.h"
#include "system/system.h"

namespace am {

BoardService::BoardService()
    : temp_sample_timer_([this](pw::chrono::SystemClock::time_point) {
        TempSampleCallback();
      }) {}

void BoardService::Init(Worker& worker, Board& board) {
  worker_ = &worker;
  board_ = &board;
}

pw::Status BoardService::Reboot(const board_RebootRequest& request,
                                pw_protobuf_Empty& /*response*/) {
  return board_->Reboot(request.reboot_type);
}

pw::Status BoardService::OnboardTemp(const pw_protobuf_Empty& /*request*/,
                                     board_OnboardTempResponse& response) {
  response.temp = board_->ReadInternalTemperature();
  return pw::OkStatus();
}

void BoardService::OnboardTempStream(
    const board_OnboardTempStreamRequest& request,
    ServerWriter<board_OnboardTempResponse>& writer) {
  if (request.sample_interval_ms < 100) {
    writer.Finish(pw::Status::InvalidArgument());
    return;
  }

  temp_sample_interval_ = pw::chrono::SystemClock::for_at_least(
      std::chrono::milliseconds(request.sample_interval_ms));
  temp_sample_writer_ = std::move(writer);

  ScheduleTempSample();
}

void BoardService::TempSampleCallback() {
  float temp = board_->ReadInternalTemperature();

  pw::Status status = temp_sample_writer_.Write({.temp = temp});
  if (status.ok()) {
    ScheduleTempSample();
  } else {
    PW_LOG_INFO("Temperature stream closed; ending periodic sampling");
  }
}

void BoardService::ScheduleTempSample() {
  worker_->RunOnce(
    [this]() { temp_sample_timer_.InvokeAfter(temp_sample_interval_); });
}

}  // namespace am

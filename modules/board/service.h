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

#include "modules/board/board.h"
#include "modules/board/board.rpc.pb.h"
#include "modules/worker/worker.h"
#include "pw_chrono/system_timer.h"
#include "pw_status/status.h"

namespace am {

class BoardService final
    : public ::board::pw_rpc::nanopb::Board::Service<BoardService> {
 public:
  BoardService();

  void Init(Worker& worker, Board& board);

  pw::Status Reboot(const board_RebootRequest& request,
                    pw_protobuf_Empty& /*response*/);

  pw::Status OnboardTemp(const pw_protobuf_Empty& /*request*/,
                         board_OnboardTempResponse& response);

  void OnboardTempStream(const board_OnboardTempStreamRequest& request,
                         ServerWriter<board_OnboardTempResponse>& writer);

 private:
  void TempSampleCallback();

  void ScheduleTempSample();

  Worker* worker_ = nullptr;
  Board* board_ = nullptr;
  pw::chrono::SystemTimer temp_sample_timer_;
  pw::chrono::SystemClock::duration temp_sample_interval_;
  ServerWriter<board_OnboardTempResponse> temp_sample_writer_;
};

}  // namespace am

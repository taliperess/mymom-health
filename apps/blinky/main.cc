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
#include <chrono>
#define PW_LOG_MODULE_NAME "MAIN"

#include <mutex>

#include "apps/blinky/blinky_pb/blinky.rpc.pb.h"
#include "pw_board_led/led.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono/system_timer.h"
#include "pw_log/log.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_system/rpc_server.h"
#include "pw_system/work_queue.h"

namespace {

using namespace ::std::chrono_literals;

void ToggleLedCallback(pw::chrono::SystemClock::time_point);

pw::sync::InterruptSpinLock blink_lock;
pw::chrono::SystemTimer blink_timer(ToggleLedCallback);
pw::chrono::SystemClock::duration blink_interval = 1s;
uint32_t num_toggles = 0;

void ScheduleLedToggle() { blink_timer.InvokeAfter(blink_interval); }

void ToggleLedCallback(pw::chrono::SystemClock::time_point) {
  PW_LOG_INFO("Toggling LED");

  std::lock_guard lock(blink_lock);

  pw::board_led::Toggle();
  --num_toggles;

  // Scheduling the timer again might not be safe from this context, so instead
  // just defer to the work queue.
  if (num_toggles > 0) {
    pw::system::GetWorkQueue().PushWork(ScheduleLedToggle);
  }
}

class BlinkyService final
    : public blinky::pw_rpc::nanopb::Blinky::Service<BlinkyService> {
public:
  pw::Status ToggleLed(const pw_protobuf_Empty &, pw_protobuf_Empty &) {
    std::lock_guard lock(blink_lock);
    blink_timer.Cancel();
    num_toggles = 0;
    pw::board_led::Toggle();
    return pw::OkStatus();
  }

  pw::Status Blink(const blinky_BlinkRequest &request, pw_protobuf_Empty &) {
    std::lock_guard lock(blink_lock);

    blink_timer.Cancel();

    if (request.blink_count == 0) {
      PW_LOG_INFO("Stopped blinking");
      return pw::OkStatus();
    }

    if (request.interval_ms > 0) {
      blink_interval = pw::chrono::SystemClock::for_at_least(
          std::chrono::milliseconds(request.interval_ms));
    } else {
      blink_interval = std::chrono::seconds(1);
    }

    auto actual_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(blink_interval);

    if (!request.has_blink_count) {
      PW_LOG_INFO("Blinking forever at a %ums interval",
                  static_cast<unsigned>(blink_interval.count()));
      num_toggles = std::numeric_limits<uint32_t>::max();
    } else {
      PW_LOG_INFO("Blinking %u times at a %ums interval", request.blink_count,
                  static_cast<unsigned>(actual_ms.count()));
      // Multiply by two as each blink is an on/off pair.
      num_toggles = request.blink_count * 2;
    }

    pw::system::GetWorkQueue().PushWork(ScheduleLedToggle);
    return pw::OkStatus();
  }
};

BlinkyService blinky_service;

} // namespace

namespace pw::system {

// This will run once after pw::system::Init() completes. This callback must
// return or it will block the work queue.
void UserAppInit() {
  pw::board_led::Init();
  pw::system::GetRpcServer().RegisterService(blinky_service);
  PW_LOG_INFO("Started blinky app; waiting for RPCs...");
}

} // namespace pw::system

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

#include "modules/morse_code/encoder.h"

#include <cctype>
#include <mutex>

#include "pw_function/function.h"
#include "pw_log/log.h"

namespace am {

Encoder::Encoder() : timer_(pw::bind_member<&Encoder::ToggleLed>(this)) {}

Encoder::~Encoder() { timer_.Cancel(); }

void Encoder::Init(Worker& worker, MonochromeLed& led) {
  worker_ = &worker;
  led_ = &led;
}

pw::Status Encoder::Encode(std::string_view msg,
                           uint32_t repeat,
                           uint32_t interval_ms) {
  if (repeat == 0) {
    PW_LOG_INFO("Encoding message forever at a %ums interval", interval_ms);
    repeat = std::numeric_limits<uint32_t>::max();
  } else {
    PW_LOG_INFO(
        "Encoding message %u times at a %ums interval", repeat, interval_ms);
  }
  pw::chrono::SystemClock::duration interval =
      pw::chrono::SystemClock::for_at_least(
          std::chrono::milliseconds(interval_ms));

  timer_.Cancel();
  {
    std::lock_guard lock(lock_);
    led_->TurnOff();
    msg_ = msg;
    msg_offset_ = 0;
    repeat_ = repeat;
    interval_ = interval;
  }
  worker_->RunOnce([this]() { ScheduleUpdate(); });
  return pw::OkStatus();
}

bool Encoder::IsIdle() const {
  std::lock_guard lock(lock_);
  return repeat_ == 0 && msg_offset_ == msg_.size() && num_bits_ == 0;
}

void Encoder::ScheduleUpdate() {
  pw::chrono::SystemClock::duration interval(0);
  bool want_led_on = false;
  while (true) {
    std::lock_guard lock(lock_);
    if (num_bits_ == 0 && !EnqueueNextLocked()) {
      return;
    }
    want_led_on = (bits_ % 2) != 0;
    if (want_led_on != led_->IsOn()) {
      break;
    }
    bits_ >>= 1;
    --num_bits_;
    interval += interval_;
  }

  timer_.InvokeAfter(interval);
}

bool Encoder::EnqueueNextLocked() {
  bits_ = 0;
  num_bits_ = 0;
  char c;

  // Try to get the next character, repeating the message as requested and
  // merging consecutive whitespace characters.
  bool needs_word_break = false;
  while (true) {
    if (msg_offset_ == msg_.size()) {
      if (--repeat_ == 0) {
        return false;
      }
      needs_word_break = true;
      msg_offset_ = 0;
    }
    c = msg_[msg_offset_++];
    if (c == '\0') {
      msg_offset_ = msg_.size();
      continue;
    }
    if (!isspace(c)) {
      break;
    }
    needs_word_break = true;
  }

  if (needs_word_break) {
    // Words are separated by 7 dits worth of blanks.
    // The previous symbol ended with 3 blanks, so add 4 more.
    num_bits_ += 4;
  }

  // Encode the character.
  auto it = internal::kEncodings.find(toupper(c));
  if (it == internal::kEncodings.end()) {
    it = internal::kEncodings.find('?');
  }
  const internal::Encoding& encoding = it->second;
  bits_ |= encoding.bits << num_bits_;
  num_bits_ += encoding.num_bits;
  return num_bits_ != 0;
}

void Encoder::ToggleLed(pw::chrono::SystemClock::time_point) {
  {
    std::lock_guard lock(lock_);
    led_->Toggle();
  }
  worker_->RunOnce([this]() { ScheduleUpdate(); });
}

}  // namespace am

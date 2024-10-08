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

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "modules/morse_code/morse_code.rpc.pb.h"
#include "modules/worker/worker.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono/system_timer.h"
#include "pw_containers/flat_map.h"
#include "pw_status/status.h"
#include "pw_string/string.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"

namespace sense {
namespace internal {

struct Encoding {
  uint32_t bits = 0;

  // Letters are separated by 3 dits worth of blanks.
  // The symbol will always end with 1 blank, so add 2 more.
  uint8_t num_bits = 2;

  constexpr explicit Encoding(const char* s) { Encode(s); }

 private:
  /// Converts a string of "dits" and "dahs", i.e. '.' and '-' respectively,
  /// into a bit sequence of ons and offs.
  constexpr void Encode(const char* s) {
    if (*s == '.') {
      bits |= (0x1 << num_bits);
      num_bits += 2;
      Encode(s + 1);
    } else if (*s == '-') {
      bits |= (0x7 << num_bits);
      num_bits += 4;
      Encode(s + 1);
    }
  }
};

// clang-format off
constexpr pw::containers::FlatMap<char, Encoding, 38> kEncodings({{
  {'A', Encoding(".-") },   {'T', Encoding("-") },
  {'B', Encoding("-...") }, {'U', Encoding("..-") },
  {'C', Encoding("-.-.") }, {'V', Encoding("...-") },
  {'D', Encoding("-..") },  {'W', Encoding(".--") },
  {'E', Encoding(".") },    {'X', Encoding("-..-") },
  {'F', Encoding("..-.") }, {'Y', Encoding("-.--") },
  {'G', Encoding("--.") },  {'Z', Encoding("--..") },
  {'H', Encoding("....") }, {'0', Encoding("-----") },
  {'I', Encoding("..") },   {'1', Encoding(".----") },
  {'J', Encoding(".---") }, {'2', Encoding("..---") },
  {'K', Encoding("-.-") },  {'3', Encoding("...--") },
  {'L', Encoding(".-..") }, {'4', Encoding("....-") },
  {'M', Encoding("--") },   {'5', Encoding(".....") },
  {'N', Encoding("-.") },   {'6', Encoding("-....") },
  {'O', Encoding("---") },  {'7', Encoding("--...") },
  {'P', Encoding(".--.") }, {'8', Encoding("---..") },
  {'Q', Encoding("--.-") }, {'9', Encoding("----.") },
  {'R', Encoding(".-.") },  {'?', Encoding("..--..") },
  {'S', Encoding("...") },  {'@', Encoding(".--.-.") },
}});
// clang-format on

}  // namespace internal

class Encoder final {
 public:
  static constexpr size_t kMaxMsgLen = sizeof(morse_code_SendRequest::msg);
  static constexpr uint32_t kDefaultIntervalMs = 60;
  static constexpr pw::chrono::SystemClock::duration kDefaultInterval =
      pw::chrono::SystemClock::for_at_least(
          std::chrono::milliseconds(kDefaultIntervalMs));

  /// State of the encoder. Passed to each `OutputFunction` call.
  class State {
   public:
    State(const State&) = delete;
    State& operator=(const State&) = delete;

    /// True if this is the last LED toggle of the encoded phrase. If the
    /// encoder is repeating, this is true at the end of each repeated
    /// message.
    [[nodiscard]] bool message_finished() const {
      return msg_offset_ == msg_.size() && num_bits_ == 1;
    };

   private:
    friend class Encoder;

    constexpr State() = default;

    pw::InlineString<kMaxMsgLen> msg_;
    size_t msg_offset_ = 0;
    size_t repeat_ = 1;
    uint32_t bits_ = 0;
    size_t num_bits_ = 0;
  };

  using OutputFunction = pw::Function<void(bool turn_on, const State& status)>;

  Encoder();

  ~Encoder();

  /// Injects this object's dependencies.
  ///
  /// This method MUST be called before using any other method.
  void Init(Worker& worker, OutputFunction&& output);

  /// Queues a sequence of callbacks to emit the given message in Morse code.
  ///
  /// The message is emitted through alternating ON and OFF (true/false) calls
  /// to the `OutputFunction` provided to `Init`. The shortest ON interval is a
  /// "dit". The longest is a "dah", which is three times the interval for a
  /// "dit". The output is kept off for one "dit" interval between each symbol,
  /// three "dit" intervals between each letter, and 7 "dit" intervals between
  /// each word.
  ///
  /// @param  request       Message to emit in Morse code.
  /// @param  repeat        Number of times to repeat the message; 0 is forever
  /// @param  interval_ms   Duration of a "dit" in milliseconds.
  pw::Status Encode(std::string_view request,
                    uint32_t repeat,
                    uint32_t interval_ms) PW_LOCKS_EXCLUDED(lock_);

  /// Returns whether this instance is currently emitting a message or not.
  bool IsIdle() const PW_LOCKS_EXCLUDED(lock_);

 private:
  /// Adds a toggle callback to the work queue.
  void ScheduleUpdate() PW_LOCKS_EXCLUDED(lock_);

  /// Encodes the next character into a sequence of LED toggles.
  ///
  /// Returns whether more toggles remain, or if the message is done.
  bool EnqueueNextLocked() PW_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  /// Callback for toggling the LED.
  void ToggleLed(pw::chrono::SystemClock::time_point);

  /// Emits an "off" to all configured outputs.
  void TurnOff();

  Worker* worker_ = nullptr;
  pw::chrono::SystemTimer timer_;
  OutputFunction output_;

  mutable pw::sync::InterruptSpinLock lock_;

  State state_ PW_GUARDED_BY(lock_);
  pw::chrono::SystemClock::duration interval_ PW_GUARDED_BY(lock_) =
      kDefaultInterval;
  bool is_on_ PW_GUARDED_BY(lock_) = false;
};

}  // namespace sense

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

#include <array>
#include <cstddef>
#include <cstdint>

#include "modules/pubsub/pubsub.h"
#include "modules/pubsub/pubsub_events.h"
#include "pw_chrono/system_clock.h"
#include "pw_chrono/system_timer.h"
#include "pw_containers/vector.h"
#include "pw_function/function.h"
#include "pw_log/log.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"
#include "pw_tokenizer/nested_tokenization.h"

namespace sense {

/// A collection of concurrent, named timers.
///
/// `TimerRequest` events can be handled by subscribing to the `PubSub` and
/// passing them to the `OnTimerRequest` method, e.g.
///
/// @code{.cpp}
/// pubsub.SubscribeTo<TimerRequest>(
///     [](TimerRequest request) { event_timers.OnTimerRequest(request); });
/// @endcode
///
/// Each timer is identified by a pw_tokenizer token. If a request is handled
/// while another request for the same token is pending, the previous request is
/// replaced by the new one.
///
/// @tparam   kCapacity   The total number of timers that can be added to this
///                       object. Timers cannot be removed once added.
template <size_t kCapacity>
class EventTimers {
 public:
  using Clock = ::pw::chrono::SystemClock;
  using Token = ::pw::tokenizer::Token;

 private:
  /// Internal class that associates a timer with a token, and can publish a
  /// `TimerExpired` event when the timer fires.
  class EventTimer {
   public:
    constexpr EventTimer(PubSub& pubsub, Token token)
        : pubsub_(pubsub),
          token_(token),
          timer_(pw::bind_member<&EventTimer::OnExpiration>(this)) {}

    Token token() const { return token_; }

    /// Schedules the callback to publish the `TimerExpired` event.
    void Schedule(Clock::time_point expiry);

   private:
    /// Callback that publishes the `TimerExpired` event.
    void OnExpiration(Clock::time_point);

    PubSub& pubsub_;
    const Token token_;
    pw::chrono::SystemTimer timer_;
  };

 public:
  constexpr explicit EventTimers(PubSub& pubsub) : pubsub_(pubsub) {}

  /// Adds a timer for the given token.
  ///
  /// This does NOT schedule a timed event. Timed events are schduled by
  /// handling `TimerRequests`.
  ///
  /// @param  pubsub  Used to publish `TimerExpired` events.
  pw::Status AddEventTimer(Token token);

  /// Handles a `TimerRequest` by scheduling a timed event.
  void OnTimerRequest(TimerRequest request) PW_LOCKS_EXCLUDED(lock_);

 private:
  PubSub& pubsub_;
  pw::sync::InterruptSpinLock lock_;
  pw::Vector<EventTimer, kCapacity> timers_ PW_GUARDED_BY(lock_);
};

// Template method implementations.

template <size_t kCapacity>
pw::Status EventTimers<kCapacity>::AddEventTimer(Token token) {
  std::lock_guard lock(lock_);
  for (auto& timer : timers_) {
    if (timer.token() == token) {
      PW_LOG_WARN("Timer already exists: " PW_TOKEN_FMT(), token);
      return pw::Status::AlreadyExists();
    }
  }
  timers_.emplace_back(pubsub_, token);
  return pw::OkStatus();
}

template <size_t kCapacity>
void EventTimers<kCapacity>::OnTimerRequest(TimerRequest request) {
  PW_LOG_INFO("Adding timed event: " PW_TOKEN_FMT() " after %u seconds",
              request.token,
              request.timeout_s);
  auto expiry = pw::chrono::SystemClock::TimePointAfterAtLeast(
      std::chrono::seconds(request.timeout_s));
  std::lock_guard lock(lock_);
  for (auto& timer : timers_) {
    if (timer.token() == request.token) {
      timer.Schedule(expiry);
      return;
    }
  }
  PW_LOG_WARN("No timer found for timed event: " PW_TOKEN_FMT(), request.token);
}

template <size_t kCapacity>
void EventTimers<kCapacity>::EventTimer::Schedule(Clock::time_point expiry) {
  timer_.InvokeAt(expiry);
}

template <size_t kCapacity>
void EventTimers<kCapacity>::EventTimer::OnExpiration(Clock::time_point) {
  PW_LOG_INFO("Timed event triggered: " PW_TOKEN_FMT(), token_);
  pubsub_.Publish(TimerExpired{.token = token_});
}

}  // namespace sense

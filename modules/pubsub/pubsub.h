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

#include <algorithm>
#include <mutex>
#include <optional>
#include <type_traits>
#include <variant>

#include "modules/worker/worker.h"
#include "pw_containers/inline_deque.h"
#include "pw_function/function.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"

namespace sense {

template <typename EventType>
class GenericPubSub {
 public:
  using Event = EventType;
  using SubscribeCallback = pw::Function<void(Event)>;
  using SubscribeToken = size_t;

  struct Subscriber {
    SubscribeToken token = kUnassignedSubscribeToken;
    SubscribeCallback callback = nullptr;
  };

  GenericPubSub(Worker& worker,
                pw::InlineDeque<Event>& event_queue,
                pw::span<Subscriber> subscribers)
      : worker_(&worker),
        event_queue_(&event_queue),
        subscribers_(subscribers),
        subscriber_count_(0),
        // Begin tokens at 1 as `kUnassignedSubscribeToken` is 0.
        next_token_(1) {}

  /// Attempts to push an event to the event queue, returning whether it was
  /// successfully published. This is both thread safe and interrupt safe.
  [[nodiscard]] bool PublishFromInterrupt(Event event) {
    if (event_lock_.try_lock()) {
      bool result = PublishLocked(event);
      event_lock_.unlock();
      return result;
    }
    return false;
  }

  /// Attempts to push an event to the event queue, returning whether it was
  /// successfully published. Unlike `PublishFromInterrupt`, this method will
  /// block until it acquires the event queue lock. This is thread safe, but it
  /// is not interrupt safe.
  [[nodiscard]] bool Publish(Event event) {
    std::lock_guard lock(event_lock_);
    return PublishLocked(event);
  }

  /// Registers a callback to be run when events are received.
  /// If registration was successful, returns a token which can be used to
  /// unsubscribe.
  ///
  /// All subscribed callbacks are invoked from the context of the work queue
  /// provided to the constructor. Callbacks should avoid long blocking
  /// operations to not starve other callbacks or work queue tasks.
  [[nodiscard]] std::optional<SubscribeToken> Subscribe(
      SubscribeCallback&& callback) {
    std::lock_guard lock(subscribers_lock_);

    auto subscriber =
        std::find_if(subscribers_.begin(), subscribers_.end(), [](auto& s) {
          return s.token == kUnassignedSubscribeToken;
        });
    if (subscriber == subscribers_.end()) {
      return std::nullopt;
    }

    SubscribeToken token = GenerateToken();

    *subscriber = {
        .token = token,
        .callback = std::move(callback),
    };
    subscriber_count_++;
    return token;
  }

  /// If the Event is a std::variant, subscribes to only events of one type.
  ///
  /// This is currently equivalent to checking std::holds_alternative before
  /// invoking the callback, buy may be optimized later.
  template <typename VariantType, typename Function>
  [[nodiscard]] std::optional<SubscribeToken> SubscribeTo(Function&& function) {
    static_assert(
        IsVariant<Event>(),
        "SubscribeTo may only be called when the event type is a std::variant");
    return Subscribe([f = std::forward<Function>(function)](Event event) {
      if (std::holds_alternative<VariantType>(event)) {
        f(std::get<VariantType>(event));
      }
    });
  }

  /// Unregisters a previously registered subscriber.
  bool Unsubscribe(SubscribeToken token) {
    std::lock_guard lock(subscribers_lock_);

    auto subscriber = std::find_if(
        subscribers_.begin(), subscribers_.end(), [token](auto& s) {
          return s.token == token;
        });
    if (subscriber == subscribers_.end()) {
      return false;
    }

    subscriber->token = kUnassignedSubscribeToken;
    subscriber->callback = nullptr;

    subscriber_count_--;
    return true;
  }

  constexpr size_t max_subscribers() const PW_NO_LOCK_SAFETY_ANALYSIS {
    return subscribers_.size();
  }
  constexpr size_t subscriber_count() const PW_NO_LOCK_SAFETY_ANALYSIS {
    return subscriber_count_;
  }

 private:
  template <typename T>
  struct IsVariant : std::false_type {};

  template <typename... Types>
  struct IsVariant<std::variant<Types...>> : std::true_type{};

  // Events (or their variant elements) must be standard layout and trivially
  // copyable & destructible.
  template <typename T>
  static constexpr bool kValidEvent =
      std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;

  template <typename T>
  struct EventsAreValid : std::bool_constant<kValidEvent<T>> {};

  template <typename... Types>
  struct EventsAreValid<std::variant<Types...>>
      : std::bool_constant<(kValidEvent<Types> && ...)> {};

  static_assert(
      EventsAreValid<Event>(),
      "Events or their std::variant elements must be standard layout, "
      "trivially copyable, and trivially destructible");

  static constexpr SubscribeToken kUnassignedSubscribeToken = SubscribeToken(0);

  SubscribeToken GenerateToken()
      PW_EXCLUSIVE_LOCKS_REQUIRED(subscribers_lock_) {
    size_t token = next_token_++;
    if (token == kUnassignedSubscribeToken) {
      token = next_token_++;
    }
    return SubscribeToken(token);
  }

  bool PublishLocked(Event event) PW_EXCLUSIVE_LOCKS_REQUIRED(event_lock_) {
    if (event_queue_->full()) {
      return false;
    }

    event_queue_->push_back(event);
    worker_->RunOnce([this]() { NotifySubscribers(); });
    return true;
  }

  void NotifySubscribers() {
    event_lock_.lock();

    if (event_queue_->empty()) {
      event_lock_.unlock();
      return;
    }

    // Copy the event out of the queue so that the lock does not have to be held
    // while running subscriber callbacks.
    Event event = event_queue_->front();
    event_queue_->pop_front();
    event_lock_.unlock();

    for (size_t i = 0; i < max_subscribers(); ++i) {
      subscribers_lock_.lock();
      if (subscribers_[i].token == kUnassignedSubscribeToken) {
        subscribers_lock_.unlock();
        continue;
      }

      // As long as `token` is assigned, this subscriber cannot be overriden, so
      // it is safe to hold onto this reference without the lock.
      Subscriber& subscriber = subscribers_[i];
      subscribers_lock_.unlock();

      subscriber.callback(event);
    }
  }

  Worker* worker_;

  pw::sync::InterruptSpinLock event_lock_;
  pw::InlineDeque<Event>* event_queue_ PW_GUARDED_BY(event_lock_);

  pw::sync::InterruptSpinLock subscribers_lock_;
  pw::span<Subscriber> subscribers_ PW_GUARDED_BY(subscribers_lock_);
  size_t subscriber_count_ PW_GUARDED_BY(subscribers_lock_);
  size_t next_token_ PW_GUARDED_BY(subscribers_lock_);
};

template <typename Event, size_t kMaxEvents, size_t kMaxSubscribers>
class GenericPubSubBuffer : public GenericPubSub<Event> {
 public:
  using Subscriber = typename GenericPubSub<Event>::Subscriber;
  using SubscribeCallback = typename GenericPubSub<Event>::SubscribeCallback;
  using SubscribeToken = typename GenericPubSub<Event>::SubscribeToken;

  constexpr GenericPubSubBuffer(Worker& worker)
      : GenericPubSub<Event>(worker, event_queue_, subscribers_) {}

 private:
  pw::InlineDeque<Event, kMaxEvents> event_queue_;
  std::array<Subscriber, kMaxSubscribers> subscribers_;
};

}  // namespace sense

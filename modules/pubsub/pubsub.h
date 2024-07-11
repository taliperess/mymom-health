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

namespace am {

template <typename Event>
class GenericPubSub {
 public:
  using SubscribeCallback = pw::Function<void(Event)>;
  using SubscribeToken = size_t;

  GenericPubSub(Worker& worker,
                pw::InlineDeque<Event>& event_queue,
                pw::span<SubscribeCallback> subscribers)
      : worker_(&worker),
        event_queue_(&event_queue),
        subscribers_(subscribers),
        subscriber_count_(0) {}

  /// Attempts to push an event to the event queue, returning whether it was
  /// successfully published. This is both thread safe and interrupt safe.
  bool Publish(Event event) {
    if (event_lock_.try_lock()) {
      bool result = PublishLocked(event);
      event_lock_.unlock();
      return result;
    }
    return false;
  }

  /// Registers a callback to be run when events are received.
  /// If registration was successful, returns a token which can be used to
  /// unsubscribe.
  ///
  /// All subscribed callbacks are invoked from the context of the work queue
  /// provided to the constructor. Callbacks should avoid long blocking
  /// operations to not starve other callbacks or work queue tasks.
  std::optional<SubscribeToken> Subscribe(SubscribeCallback&& callback) {
    std::lock_guard lock(subscribers_lock_);

    auto slot = std::find_if(subscribers_.begin(),
                             subscribers_.end(),
                             [](auto& s) { return s == nullptr; });
    if (slot == subscribers_.end()) {
      return std::nullopt;
    }

    *slot = std::move(callback);
    subscriber_count_++;
    return SubscribeToken(slot - subscribers_.begin());
  }

  /// Unregisters a previously registered subscriber.
  bool Unsubscribe(SubscribeToken token) {
    std::lock_guard lock(subscribers_lock_);

    const size_t index = static_cast<size_t>(token);
    if (index >= subscribers_.size()) {
      return false;
    }

    subscribers_[index] = nullptr;
    subscriber_count_--;
    return true;
  }

  constexpr size_t max_subscribers() const PW_NO_LOCK_SAFETY_ANALYSIS {
    return subscribers_.size();
  }
  constexpr size_t subscriber_count() const { return subscriber_count_; }

 private:
  // Events (or their variant elements) must be standard layout and trivially
  // copyable & destructible.
  template <typename T>
  static constexpr bool kValidEvent =
      std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T> &&
      std::is_standard_layout_v<T>;

  template <typename T>
  struct EventsAreValid : std::bool_constant<kValidEvent<T>> {};

  template <typename... Types>
  struct EventsAreValid<std::variant<Types...>>
      : std::bool_constant<(kValidEvent<Types> && ...)> {};

  static_assert(
      EventsAreValid<Event>(),
      "Events or their std::variant elements must be standard layout, "
      "trivially copyable, and trivially destructible");

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

    subscribers_lock_.lock();
    for (auto& callback : subscribers_) {
      if (callback != nullptr) {
        callback(event);
      }
    }
    subscribers_lock_.unlock();
  }

  Worker* worker_;

  pw::sync::InterruptSpinLock event_lock_;
  pw::InlineDeque<Event>* event_queue_ PW_GUARDED_BY(event_lock_);

  pw::sync::InterruptSpinLock subscribers_lock_;
  pw::span<SubscribeCallback> subscribers_ PW_GUARDED_BY(subscribers_lock_);
  size_t subscriber_count_;
};

template <typename Event, size_t kMaxEvents, size_t kMaxSubscribers>
class GenericPubSubBuffer : public GenericPubSub<Event> {
 public:
  using SubscribeCallback = typename GenericPubSub<Event>::SubscribeCallback;
  using SubscribeToken = typename GenericPubSub<Event>::SubscribeToken;

  constexpr GenericPubSubBuffer(Worker& worker)
      : GenericPubSub<Event>(worker, event_queue_, subscribers_) {}

 private:
  pw::InlineDeque<Event, kMaxEvents> event_queue_;
  std::array<SubscribeCallback, kMaxSubscribers> subscribers_;
};

}  // namespace am

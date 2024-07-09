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

#include "pw_function/function.h"
#include "pw_sync/binary_semaphore.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_work_queue/work_queue.h"

namespace am {
namespace internal {

template <typename Event>
class PubSubBase {
 public:
  using SubscribeCallback = pw::Function<void(Event)>;
  using SubscribeToken = size_t;

  template <
      typename = std::enable_if_t<std::is_trivially_copyable_v<Event> &&
                                  std::is_trivially_destructible_v<Event>>>
  PubSubBase(pw::work_queue::WorkQueue& work_queue,
             pw::span<SubscribeCallback> subscribers)
      : work_queue_(&work_queue),
        subscribers_(subscribers),
        subscriber_count_(0) {
    event_lock_.release();
  }

  /// Pushes an event to the event queue, blocking until the event is
  /// accepted. This is **NOT** interrupt safe.
  void Publish(Event event) {
    // Wait for the current event to be finished processing.
    event_lock_.acquire();
    PublishLocked(event);
  }

  /// Attempts to push an event to the event queue, returning whether it was
  /// successfully published.
  /// The `PubSub` takes ownership of the event.
  bool TryPublish(Event event) {
    if (!event_lock_.try_acquire()) {
      return false;
    }

    PublishLocked(event);
    return true;
  }

  /// Registers a callback to be run when events are received.
  /// If registration was successful, returns a token which can be used to
  /// unsubscribe.
  ///
  /// All subscribed callbacks are invoked from the context of the work queue
  /// provided to the constructor, while a lock is held.
  /// As a result, certain operations such as publishing to the work queue are
  /// disallowed, and generally, callbacks should avoid long blocking operations
  /// to not starve other callbacks or work queue tasks.
  std::optional<SubscribeToken> Subscribe(SubscribeCallback&& callback) {
    std::lock_guard lock(subscriber_lock_);

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
    std::lock_guard lock(subscriber_lock_);

    const size_t index = static_cast<size_t>(token);
    if (index >= subscribers_.size()) {
      return false;
    }

    subscribers_[index] = nullptr;
    subscriber_count_--;
    return true;
  }

  constexpr size_t max_subscribers() const { return subscribers_.size(); }
  constexpr size_t subscriber_count() const { return subscriber_count_; }

 private:
  void PublishLocked(Event event) {
    queued_event_ = event;
    work_queue_->PushWork([this]() { NotifySubscribers(); });
  }

  void NotifySubscribers() {
    subscriber_lock_.lock();
    for (auto& callback : subscribers_) {
      if (callback != nullptr) {
        callback(queued_event_);
      }
    }
    subscriber_lock_.unlock();

    // Allow new events to be processed.
    event_lock_.release();
  }

  pw::work_queue::WorkQueue* work_queue_;

  // Lock protecting the event queue. Uses a semaphore as it is acquired and
  // released by different threads.
  pw::sync::BinarySemaphore event_lock_;
  Event queued_event_;

  pw::sync::InterruptSpinLock subscriber_lock_;
  pw::span<SubscribeCallback> subscribers_;
  size_t subscriber_count_;
};

}  // namespace internal

// TODO
struct Event {};

template <size_t kMaxSubscribers>
class PubSub : public internal::PubSubBase<Event> {
 public:
  using PubSubBase::SubscribeCallback;
  using PubSubBase::SubscribeToken;

  constexpr PubSub(pw::work_queue::WorkQueue& work_queue)
      : PubSubBase(work_queue, subscribers_) {}

 private:
  std::array<SubscribeCallback, kMaxSubscribers> subscribers_;
};

}  // namespace am

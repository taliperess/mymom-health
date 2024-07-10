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

#include "modules/worker/worker.h"
#include "pw_function/function.h"
#include "pw_log/log.h"
#include "pw_thread/test_thread_context.h"
#include "pw_thread/thread.h"
#include "pw_unit_test/framework.h"
#include "pw_work_queue/work_queue.h"

namespace am {
namespace internal {

/// A worker which delegates to a work queue running on a dedicated test
/// thread. Callers should use ``TestWorker`` instead.
class GenericTestWorker : public Worker {
 public:
  GenericTestWorker(pw::work_queue::WorkQueue& work_queue);
  GenericTestWorker(const GenericTestWorker&) = delete;
  GenericTestWorker& operator=(const GenericTestWorker&) = delete;
  GenericTestWorker(GenericTestWorker&&) = delete;
  GenericTestWorker& operator=(GenericTestWorker&&) = delete;

  void RunOnce(pw::Function<void()>&& work) final;

  // Stops the work queue. This method MUST be called before leaving the test
  // body. Otherwise, the work queue may reference objects that have gone out of
  // scope.
  void Stop();

 protected:
  ~GenericTestWorker();

 private:
  pw::work_queue::WorkQueue* work_queue_ = nullptr;
  pw::thread::test::TestThreadContext context_;
  pw::thread::Thread work_thread_;
};

}  // namespace internal

/// A worker which delegates to a work queue running on a dedicated test
/// thread.
template <size_t kBufferSize = 10>
class TestWorker final : public internal::GenericTestWorker {
 public:
  TestWorker() : GenericTestWorker(work_queue_) {}

 private:
  pw::work_queue::WorkQueueWithBuffer<kBufferSize> work_queue_;
};

}  // namespace am
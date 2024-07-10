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

#include "modules/worker/test_worker.h"

#include "pw_assert/check.h"

namespace am::internal {

GenericTestWorker::GenericTestWorker(pw::work_queue::WorkQueue& work_queue)
    : work_queue_(&work_queue),
      work_thread_(pw::thread::Thread(context_.options(), work_queue)) {}

void GenericTestWorker::RunOnce(pw::Function<void()>&& work) {
  work_queue_->PushWork(std::move(work));
}

GenericTestWorker::~GenericTestWorker() {
  PW_CHECK_PTR_EQ(
      work_queue_,
      nullptr,
      "`TestWorker::Stop` must be called before the test completes.");
}

void GenericTestWorker::Stop() {
  work_queue_->RequestStop();
  work_thread_.join();
  work_queue_ = nullptr;
}

}  // namespace am::internal

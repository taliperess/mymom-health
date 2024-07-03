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

#include "modules/testing/work_queue.h"

#include "pw_assert/check.h"

namespace am::internal {

GenericTestWithWorkQueue::GenericTestWithWorkQueue(
    pw::work_queue::WorkQueue& work_queue)
    : work_queue_(&work_queue) {}

void GenericTestWithWorkQueue::SetUp() {
  work_thread_ = pw::thread::Thread(context_.options(), *work_queue_);
}

void GenericTestWithWorkQueue::StopWorkQueue() {
  work_queue_->RequestStop();
  work_thread_.join();
  work_queue_ = nullptr;
}

void GenericTestWithWorkQueue::TearDown() {
  PW_CHECK_PTR_EQ(work_queue_,
                  nullptr,
                  "StopWorkQueue must be called before the test completes.");
}

}  // namespace am::internal

// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rsocket/tck-test/BaseSubscriber.h"

#include <thread>

#include <folly/Format.h>

using namespace folly;

namespace rsocket {
namespace tck {

void BaseSubscriber::awaitTerminalEvent() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!terminatedCV_.wait_for(lock, std::chrono::seconds(5), [&] {
        return completed_ || errored_;
      })) {
    throw std::runtime_error("Timed out while waiting for terminating event");
  }
}

void BaseSubscriber::awaitAtLeast(int numItems) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!valuesCV_.wait_for(lock, std::chrono::seconds(5), [&] {
        return valuesCount_ >= numItems;
      })) {
    throw std::runtime_error("Timed out while waiting for items");
  }
}

void BaseSubscriber::awaitNoEvents(int waitTime) {
  const int valuesCount = valuesCount_;
  const bool completed = completed_;
  const bool errored = errored_;
  /* sleep override */
  std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
  if (valuesCount != valuesCount_ || completed != completed_ ||
      errored != errored_) {
    throw std::runtime_error(
        folly::sformat("Events occured within {}ms", waitTime));
  }
}

void BaseSubscriber::assertNoErrors() {
  if (errored_) {
    throw std::runtime_error("Subscription completed with unexpected errors");
  }
}

void BaseSubscriber::assertError() {
  if (!errored_) {
    throw std::runtime_error("Subscriber did not receive onError");
  }
}

void BaseSubscriber::assertValues(
    const std::vector<std::pair<std::string, std::string>>& values) {
  assertValueCount(values.size());
  const std::unique_lock<std::mutex> lock(mutex_);
  for (size_t i = 0; i < values.size(); i++) {
    if (values_[i] != values[i]) {
      throw std::runtime_error(folly::sformat(
          "Unexpected element {}:{}.  Expected element {}:{}",
          values_[i].first,
          values_[i].second,
          values[i].first,
          values[i].second));
    }
  }
}

void BaseSubscriber::assertValueCount(size_t valueCount) {
  const std::unique_lock<std::mutex> lock(mutex_);
  if (values_.size() != valueCount) {
    throw std::runtime_error(folly::sformat(
        "Did not receive expected number of values! Expected={} Actual={}",
        valueCount,
        values_.size()));
  }
}

void BaseSubscriber::assertReceivedAtLeast(size_t valueCount) {
  const std::unique_lock<std::mutex> lock(mutex_);
  if (values_.size() < valueCount) {
    throw std::runtime_error(folly::sformat(
        "Did not receive the minimum number of values! Expected={} Actual={}",
        valueCount,
        values_.size()));
  }
}

void BaseSubscriber::assertCompleted() {
  if (!completed_) {
    throw std::runtime_error("Subscriber did not completed");
  }
}

void BaseSubscriber::assertNotCompleted() {
  if (completed_) {
    throw std::runtime_error("Subscriber unexpectedly completed");
  }
}

void BaseSubscriber::assertCanceled() {
  if (!canceled_) {
    throw std::runtime_error("Subscription should be canceled");
  }
}

} // namespace tck
} // namespace rsocket

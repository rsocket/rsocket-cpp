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

#include "rsocket/examples/util/ExampleSubscriber.h"
#include <iostream>

using namespace ::rsocket;

namespace rsocket_example {

ExampleSubscriber::~ExampleSubscriber() {
  LOG(INFO) << "ExampleSubscriber destroy " << this;
}

ExampleSubscriber::ExampleSubscriber(int initialRequest, int numToTake)
    : initialRequest_(initialRequest),
      thresholdForRequest_(initialRequest * 0.75),
      numToTake_(numToTake),
      received_(0) {
  LOG(INFO) << "ExampleSubscriber " << this << " created with => "
            << "  Initial Request: " << initialRequest
            << "  Threshold for re-request: " << thresholdForRequest_
            << "  Num to Take: " << numToTake;
}

void ExampleSubscriber::onSubscribe(
    std::shared_ptr<yarpl::flowable::Subscription> subscription) noexcept {
  LOG(INFO) << "ExampleSubscriber " << this << " onSubscribe, requesting "
            << initialRequest_;
  subscription_ = std::move(subscription);
  requested_ = initialRequest_;
  subscription_->request(initialRequest_);
}

void ExampleSubscriber::onNext(Payload element) noexcept {
  LOG(INFO) << "ExampleSubscriber " << this
            << " onNext as string: " << element.moveDataToString();
  received_++;
  if (--requested_ == thresholdForRequest_) {
    int toRequest = (initialRequest_ - thresholdForRequest_);
    LOG(INFO) << "ExampleSubscriber " << this << " requesting " << toRequest
              << " more items";
    requested_ += toRequest;
    subscription_->request(toRequest);
  };
  if (received_ == numToTake_) {
    LOG(INFO) << "ExampleSubscriber " << this << " cancelling after receiving "
              << received_ << " items.";
    subscription_->cancel();
  }
}

void ExampleSubscriber::onComplete() noexcept {
  LOG(INFO) << "ExampleSubscriber " << this << " onComplete";
  terminated_ = true;
  terminalEventCV_.notify_all();
}

void ExampleSubscriber::onError(folly::exception_wrapper ex) noexcept {
  LOG(ERROR) << "ExampleSubscriber " << this << " onError: " << ex;
  terminated_ = true;
  terminalEventCV_.notify_all();
}

void ExampleSubscriber::awaitTerminalEvent() {
  LOG(INFO) << "ExampleSubscriber " << this << " block thread";
  // now block this thread
  std::unique_lock<std::mutex> lk(m_);
  // if shutdown gets implemented this would then be released by it
  terminalEventCV_.wait(lk, [this] { return terminated_; });
  LOG(INFO) << "ExampleSubscriber " << this << " unblocked";
}
} // namespace rsocket_example

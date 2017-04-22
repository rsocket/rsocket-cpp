// Copyright 2004-present Facebook. All Rights Reserved.

#include "examples/util/ExampleSubscriber.h"

using namespace ::reactivesocket;

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
        yarpl::Reference<yarpl::Subscription> subscription) noexcept {
  LOG(INFO) << "ExampleSubscriber " << this << " onSubscribe";
  subscription_ = std::move(subscription);
  requested_ = initialRequest_;
  subscription_->request(initialRequest_);
}

void ExampleSubscriber::onNext(const Payload& element) noexcept {
  LOG(INFO) << "ExampleSubscriber " << this
            << " onNext as string: " << element.cloneDataToString();
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

void ExampleSubscriber::onError(const std::exception_ptr ex) noexcept {
  LOG(INFO) << "ExampleSubscriber " << this << " onError";
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
}

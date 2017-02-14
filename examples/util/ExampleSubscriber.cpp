// Copyright 2004-present Facebook. All Rights Reserved.

#include "examples/util/ExampleSubscriber.h"

using namespace ::reactivesocket;

namespace rsocket_example {

ExampleSubscriber::~ExampleSubscriber() {
  LOG(INFO) << "ExampleSubscriber destroy " << this;
}

ExampleSubscriber::ExampleSubscriber(int initialRequest, int numToTake)
    : initialRequest(initialRequest),
      thresholdForRequest(initialRequest * 0.75),
      numToTake(numToTake),
      received(0) {
  LOG(INFO) << "ExampleSubscriber " << this << " created with => "
            << "  Initial Request: " << initialRequest
            << "  Threshold for re-request: " << thresholdForRequest
            << "  Num to Take: " << numToTake;
}

void ExampleSubscriber::onSubscribe(
    std::shared_ptr<Subscription> subscription) noexcept {
  LOG(INFO) << "ExampleSubscriber " << this << " onSubscribe";
  subscription_ = std::move(subscription);
  requested = initialRequest;
  subscription_->request(initialRequest);
}

void ExampleSubscriber::onNext(Payload element) noexcept {
  LOG(INFO) << "ExampleSubscriber " << this
            << " onNext as string: " << element.moveDataToString();
  received++;
  if (--requested == thresholdForRequest) {
    int toRequest = (initialRequest - thresholdForRequest);
    LOG(INFO) << "ExampleSubscriber " << this << " requesting " << toRequest
              << " more items";
    requested += toRequest;
    subscription_->request(toRequest);
  };
  if (received == numToTake) {
    LOG(INFO) << "ExampleSubscriber " << this << " cancelling after receiving "
              << received << " items.";
    subscription_->cancel();
  }
}

void ExampleSubscriber::onComplete() noexcept {
  LOG(INFO) << "ExampleSubscriber " << this << " onComplete";
  terminated = true;
  terminalEventCV_.notify_all();
}

void ExampleSubscriber::onError(folly::exception_wrapper ex) noexcept {
  LOG(INFO) << "ExampleSubscriber " << this << " onError " << ex.what();
  terminated = true;
  terminalEventCV_.notify_all();
}

void ExampleSubscriber::awaitTerminalEvent() {
  // now block this thread
  std::unique_lock<std::mutex> lk(m);
  // if shutdown gets implemented this would then be released by it
  terminalEventCV_.wait(lk, [this] { return terminated; });
}
}

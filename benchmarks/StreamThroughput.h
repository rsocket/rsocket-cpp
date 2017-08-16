// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/RSocketResponder.h"

namespace rsocket {

/// Responder that streams back a fixed message infinitely.
class FixedResponder : public RSocketResponder {
 public:
  FixedResponder(const std::string& message)
      : message_{folly::IOBuf::copyBuffer(message)} {}

  yarpl::Reference<yarpl::flowable::Flowable<Payload>> handleRequestStream(
      Payload,
      StreamId) override {
    return yarpl::flowable::Flowables::fromGenerator<Payload>(
        [msg = message_->clone()] { return Payload(msg->clone()); });
  }

 private:
  std::unique_ptr<folly::IOBuf> message_;
};

/// Subscriber that requests N items and cancels the subscription once all of
/// them arrive.
class BoundedSubscriber : public yarpl::flowable::Subscriber<Payload> {
 public:
  explicit BoundedSubscriber(size_t requested) : requested_{requested} {}

  void onSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> subscription) override {
    subscription_ = std::move(subscription);
    subscription_->request(requested_);
  }

  void onNext(Payload) override {
    if (received_.fetch_add(1) == requested_ - 1) {
      subscription_->cancel();
      baton_.post();
    }
  }

  void onComplete() override {
    baton_.post();
  }

  void onError(folly::exception_wrapper) override {
    baton_.post();
  }

  void awaitTerminalEvent() {
    baton_.wait();
  }

 private:
  size_t requested_{0};
  yarpl::Reference<yarpl::flowable::Subscription> subscription_;
  folly::Baton<std::atomic, false /* SinglePoster */> baton_;
  std::atomic<size_t> received_{0};
};
}

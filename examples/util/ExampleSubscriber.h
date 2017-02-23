// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

using namespace ::reactivesocket;

/**
 * Subscriber that logs all events.
 * Request 5 items to begin with, then 3 more after each receipt of 3.
 */
namespace rsocket_example {
class ExampleSubscriber : public Subscriber<Payload> {
 public:
  ~ExampleSubscriber();
  ExampleSubscriber(int initialRequest, int numToTake);

  void onSubscribe(
      std::shared_ptr<Subscription> subscription) noexcept override;
  void onNext(Payload element) noexcept override;
  void onComplete() noexcept override;
  void onError(folly::exception_wrapper ex) noexcept override;

 private:
  int initialRequest;
  int thresholdForRequest;
  int numToTake;
  int requested;
  int received;
  std::shared_ptr<Subscription> subscription_;
};
}

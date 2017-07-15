// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <condition_variable>
#include <mutex>
#include "rsocket/Payload.h"

#include "yarpl/Flowable.h"
#include "yarpl/flowable/Subscriber.h"

/**
 * Subscriber that logs all events.
 * Request 5 items to begin with, then 3 more after each receipt of 3.
 */
namespace rsocket_example {
class ExampleSubscriber : public yarpl::flowable::Subscriber<rsocket::Payload> {
 public:
  ~ExampleSubscriber();
  ExampleSubscriber(int initialRequest, int numToTake);

  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>
                       subscription) noexcept override;
  void onNext(rsocket::Payload) noexcept override;
  void onComplete() noexcept override;
  void onError(std::exception_ptr ex) noexcept override;

  void awaitTerminalEvent();

 private:
  int initialRequest_;
  int thresholdForRequest_;
  int numToTake_;
  int requested_;
  int received_;
  yarpl::Reference<yarpl::flowable::Subscription> subscription_;
  bool terminated_{false};
  std::mutex m_;
  std::condition_variable terminalEventCV_;
};
}

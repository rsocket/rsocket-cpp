// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <condition_variable>
#include <mutex>
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

/**
 * Subscriber that logs all events.
 * Request 5 items to begin with, then 3 more after each receipt of 3.
 */
namespace rsocket_example {
class ExampleSubscriber
    : public reactivesocket::Subscriber<reactivesocket::Payload> {
 public:
  ~ExampleSubscriber();
  ExampleSubscriber(int initialRequest, int numToTake);

  void onSubscribe(std::shared_ptr<reactivesocket::Subscription>
                       subscription) noexcept override;
  void onNext(reactivesocket::Payload element) noexcept override;
  void onComplete() noexcept override;
  void onError(folly::exception_wrapper ex) noexcept override;

  void awaitTerminalEvent();

 private:
  int initialRequest_;
  int thresholdForRequest_;
  int numToTake_;
  int requested_;
  int received_;
  std::shared_ptr<reactivesocket::Subscription> subscription_;
  bool terminated_{false};
  std::mutex m_;
  std::condition_variable terminalEventCV_;
};
}

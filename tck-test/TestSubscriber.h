// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <condition_variable>
#include <mutex>
#include <vector>
#include "src/Payload.h"
#include "yarpl/flowable/Subscriber.h"

namespace rsocket {
namespace tck {

class TestSubscriber : public yarpl::flowable::Subscriber<Payload> {
 public:
  explicit TestSubscriber(int initialRequestN = 0);

  void request(int n);
  void cancel();

  void awaitTerminalEvent();
  void awaitAtLeast(int numItems);
  void awaitNoEvents(int waitTime);
  void assertNoErrors();
  void assertError();
  void assertValues(
      const std::vector<std::pair<std::string, std::string>>& values);
  void assertValueCount(size_t valueCount);
  void assertReceivedAtLeast(size_t valueCount);
  void assertCompleted();
  void assertNotCompleted();
  void assertCanceled();

 protected:
  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>
                       subscription) noexcept override;
  void onNext(Payload element) noexcept override;
  void onComplete() noexcept override;
  void onError(std::exception_ptr ex) noexcept override;

 private:
  void assertTerminated();

  yarpl::Reference<yarpl::flowable::Subscription> subscription_;
  int initialRequestN_{0};

  std::atomic<bool> canceled_{false};

  ////////////////////////////////////////////////////////////////////////////
  std::mutex mutex_; // all variables below has to be protected with the mutex

  std::vector<std::pair<std::string, std::string>> values_;
  std::condition_variable valuesCV_;
  std::atomic<int> valuesCount_{0};

  std::vector<std::exception_ptr> errors_;

  std::condition_variable terminatedCV_;
  std::atomic<bool> completed_{false}; // by onComplete
  std::atomic<bool> errored_{false}; // by onError
  ////////////////////////////////////////////////////////////////////////////
};

} // tck
} // reactivesocket

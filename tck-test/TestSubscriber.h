// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <condition_variable>
#include <folly/ExceptionWrapper.h>
#include <mutex>
#include <reactive-streams/utilities/SmartPointers.h>
#include <vector>
#include "src/ReactiveStreamsCompat.h"
#include "src/Payload.h"

namespace folly {
class EventBase;
}

namespace reactivesocket {
namespace tck {

class TestSubscriber : public reactivesocket::Subscriber<Payload> {
 public:
  explicit TestSubscriber(folly::EventBase& rsEventBase, int initialRequestN = 0);

  void request(int n);
  void cancel();

  void awaitTerminalEvent();
  void awaitAtLeast(int numItems);
  void awaitNoEvents(int numelements);
  void assertNoErrors();
  void assertError();
  void assertValues(const std::vector<std::pair<std::string, std::string>>& values);
  void assertValueCount(int valueCount);
  void assertReceivedAtLeast(int valueCount);
  void assertCompleted();
  void assertNotCompleted();
  void assertCanceled();

  void waitForInitialization();

 protected:
  void onSubscribe(Subscription& subscription) override;
  void onNext(Payload element) override;
  void onComplete() override;
  void onError(folly::exception_wrapper ex) override;

 private:
  void assertTerminated();

  SubscriptionPtr<Subscription> subscription_;
  int initialRequestN_{0};

  std::atomic<bool> canceled_{false};


  std::mutex mutex_; // all variables below has to be protected with the mutex

  std::condition_variable initializedCV_;
  std::atomic<bool> initialized_{false};


  std::vector<std::string> onNextValues_;
  std::condition_variable onNextValuesCV_;
  std::atomic<int> onNextItemsCount_{0};

  std::vector<folly::exception_wrapper> errors_;

  folly::EventBase* rsEventBase_;
};

} // tck
} // reactive socket

// Copyright 2004-present Facebook. All Rights Reserved.

#include "TestSubscriber.h"

#include <folly/io/async/EventBase.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>
#include "src/mixins/MemoryMixin.h"

using namespace folly;

namespace reactivesocket {
namespace tck {

class EventBaseSubscription : public Subscription {
 public:
  explicit EventBaseSubscription(EventBase& eventBase, Subscription& subscription) : eventBase_(&eventBase), inner_(&subscription) {
  }

  void request(size_t n) override {
    eventBase_->runInEventBaseThread([this, n](){inner_.request(n);});
  }

  void cancel() override {
    eventBase_->runInEventBaseThread([this](){inner_.cancel();});
  }

 private:
  EventBase* eventBase_{nullptr};
  SubscriptionPtr<Subscription> inner_;
};

TestSubscriber::TestSubscriber(EventBase& rsEventBase, int initialRequestN) : initialRequestN_(initialRequestN), rsEventBase_(&rsEventBase) {
}

void TestSubscriber::request(int n) {
  subscription_.request(n);
}

void TestSubscriber::cancel() {
  canceled_ = true;
  subscription_.cancel();
}

//TODO
void TestSubscriber::awaitTerminalEvent() {}

void TestSubscriber::awaitAtLeast(int numItems) {
  // Wait until onNext sends data

  std::unique_lock<std::mutex> lock(mutex_);
  if(!onNextValuesCV_.wait_for(lock, std::chrono::seconds(5), [&]{return onNextItemsCount_ >= numItems;})) {
    throw std::runtime_error("timed out while waiting for items");
  }

  LOG(INFO) << "received " << onNextItemsCount_.load() << " items; was waiting for " << numItems;
  onNextItemsCount_ = 0;
}

//TODO
void TestSubscriber::awaitNoEvents(int numelements) {}

void TestSubscriber::assertNoErrors() {
  assertTerminated();

  std::unique_lock<std::mutex> lock(mutex_);
  if (errors_.empty()) {
    LOG(INFO) << "subscription terminated without errors";
  } else {
    throw std::runtime_error("subscription completed with unexpected errors");
  }
}

//TODO
void TestSubscriber::assertError() {}
//TODO
void TestSubscriber::assertValues(const std::vector<std::pair<std::string, std::string>>& values) {}
//TODO
void TestSubscriber::assertValueCount(int valueCount) {}
//TODO
void TestSubscriber::assertReceivedAtLeast(int valueCount) {}
//TODO
void TestSubscriber::assertCompleted() {}
//TODO
void TestSubscriber::assertNotCompleted() {}

void TestSubscriber::assertCanceled() {
  if (canceled_) {
    LOG(INFO) << "verified canceled";
  } else {
    throw std::runtime_error("subscription should be canceled");
  }
}

void TestSubscriber::assertTerminated() {
  if (!canceled_) {
    throw std::runtime_error("subscription is not terminated");
  }
}

void TestSubscriber::onSubscribe(Subscription& subscription) {
  subscription_.reset(&createManagedInstance<EventBaseSubscription>(*rsEventBase_, subscription));

//  actual.onSubscribe(s);

//  if (canceled) {
//    return;
//  }

  if (initialRequestN_ > 0) {
    subscription.request(initialRequestN_);
  }

//  long mr = missedRequested.getAndSet(0L);
//  if (mr != 0L) {
//    s.request(mr);
//  }

  {
    std::unique_lock<std::mutex> lock(mutex_);
    initialized_ = true;
  }
  initializedCV_.notify_one();
}

void TestSubscriber::onNext(Payload element) {
  //TODO: add metadata
  auto data = element->moveToFbString();

  LOG(INFO) << "ON NEXT: " << data;

//  if (isEcho) {
//    echosub.add(tup);
//    return;
//  }
//  if (!checkSubscriptionOnce) {
//    checkSubscriptionOnce = true;
//    if (subscription.get() == null) {
//      errors.add(new IllegalStateException("onSubscribe not called in proper order"));
//    }
//  }
//  lastThread = Thread.currentThread();

  {
    std::unique_lock<std::mutex> lock(mutex_);
    onNextValues_.push_back(data.toStdString());
    ++onNextItemsCount_;
  }
  onNextValuesCV_.notify_one();

//  numOnNext.countDown();
//  takeLatch.countDown();

//  actual.onNext(new PayloadImpl(tup.getK(), tup.getV()));
}

void TestSubscriber::onComplete() {
//  isComplete = true;
//  if (!checkSubscriptionOnce) {
//    checkSubscriptionOnce = true;
//    if (subscription.get() == null) {
//      errors.add(new IllegalStateException("onSubscribe not called in proper order"));
//    }
//  }
//  try {
//    lastThread = Thread.currentThread();
//    completions++;
//
//    actual.onComplete();
//  } finally {
//          done.countDown();
//  }
}

void TestSubscriber::onError(folly::exception_wrapper ex) {
//  if (!checkSubscriptionOnce) {
//    checkSubscriptionOnce = true;
//    if (subscription.get() == null) {
//      errors.add(new NullPointerException("onSubscribe not called in proper order"));
//    }
//  }
//  try {
//    lastThread = Thread.currentThread();

  std::unique_lock<std::mutex> lock(mutex_);
  errors_.push_back(std::move(ex));

//
//    if (t == null) {
//      errors.add(new IllegalStateException("onError received a null Subscription"));
//    }
//
//    actual.onError(t);
//  } finally {
//          done.countDown();
//  }
}

void TestSubscriber::waitForInitialization() {
  if (initialized_) {
    return;
  }

  std::unique_lock<std::mutex> lock(mutex_);
  if(!initializedCV_.wait_for(lock, std::chrono::seconds(5), [&]{return initialized_.load();})) {
    throw std::runtime_error("timed out while initializing test subscriber");
  }
}

} // tck
} // reactive socket

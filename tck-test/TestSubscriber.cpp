// Copyright 2004-present Facebook. All Rights Reserved.

#include "TestSubscriber.h"

#include <folly/io/IOBuf.h>
#include <folly/io/async/EventBase.h>
#include <glog/logging.h>
#include "src/mixins/MemoryMixin.h"

using namespace folly;

namespace reactivesocket {
namespace tck {

class EventBaseSubscription : public Subscription {
 public:
  explicit EventBaseSubscription(
      EventBase& eventBase,
      Subscription& subscription)
      : eventBase_(&eventBase), inner_(&subscription) {}

  void request(size_t n) override {
    eventBase_->runInEventBaseThread([this, n]() { inner_.request(n); });
  }

  void cancel() override {
    eventBase_->runInEventBaseThread([this]() { inner_.cancel(); });
  }

 private:
  EventBase* eventBase_{nullptr};
  SubscriptionPtr<Subscription> inner_;
};

TestSubscriber::TestSubscriber(EventBase& rsEventBase, int initialRequestN)
    : initialRequestN_(initialRequestN), rsEventBase_(&rsEventBase) {}

void TestSubscriber::request(int n) {
  subscription_.request(n);
}

void TestSubscriber::cancel() {
  canceled_ = true;
  subscription_.cancel();
}

void TestSubscriber::awaitTerminalEvent() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!terminatedCV_.wait_for(lock, std::chrono::seconds(5), [&] {
    return completed_ || errored_;
  })) {
    throw std::runtime_error("timed out while waiting for terminating event");
  }
}

void TestSubscriber::awaitAtLeast(int numItems) {
  // Wait until onNext sends data

  std::unique_lock<std::mutex> lock(mutex_);
  if (!onNextValuesCV_.wait_for(lock, std::chrono::seconds(5), [&] {
        return onNextItemsCount_ >= numItems;
      })) {
    throw std::runtime_error("timed out while waiting for items");
  }

  LOG(INFO) << "received " << onNextItemsCount_.load()
            << " items; was waiting for " << numItems;
  onNextItemsCount_ = 0;
}

void TestSubscriber::awaitNoEvents(int numelements) {
  // TODO
  throw std::runtime_error("not implemented");
}

void TestSubscriber::assertNoErrors() {
  if (!errored_) {
    LOG(INFO) << "subscription is without errors";
  } else {
    throw std::runtime_error("subscription completed with unexpected errors");
  }
}

void TestSubscriber::assertError() {
  assertTerminated();

  if (!errored_) {
    throw std::runtime_error("subscriber did not received onError");
  }
}

void TestSubscriber::assertValues(
    const std::vector<std::pair<std::string, std::string>>& values) {
  // TODO
  throw std::runtime_error("not implemented");
}

void TestSubscriber::assertValueCount(size_t valueCount) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (onNextValues_.size() == valueCount) {
    LOG(INFO) << "received expected " << valueCount << " values.";
  } else {
    LOG(INFO) << "didn't receive expected number of values! expected="
              << valueCount << " actual=" << onNextValues_.size();
    throw std::runtime_error("didn't receive expected number of values!");
  }
}

void TestSubscriber::assertReceivedAtLeast(int valueCount) {
  // TODO
  throw std::runtime_error("not implemented");
}

void TestSubscriber::assertCompleted() {
  assertTerminated();

  if (!completed_) {
    throw std::runtime_error("subscriber did not completed");
  }
}

void TestSubscriber::assertNotCompleted() {
  if (completed_) {
    throw std::runtime_error("subscriber unexpectedly completed");
  } else {
    LOG(INFO) << "subscriber is not completed";
  }
}

void TestSubscriber::assertCanceled() {
  if (canceled_) {
    LOG(INFO) << "verified canceled";
  } else {
    throw std::runtime_error("subscription should be canceled");
  }
}

void TestSubscriber::assertTerminated() {
  if (!completed_ && ! errored_) {
    throw std::runtime_error("subscription is not terminated yet. This is most likely a bug in the test.");
  }
}

void TestSubscriber::onSubscribe(Subscription& subscription) {
  subscription_.reset(&createManagedInstance<EventBaseSubscription>(
      *rsEventBase_, subscription));

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
}

void TestSubscriber::onNext(Payload element) {
  LOG(INFO) << "ON NEXT: " << element;

  //  if (isEcho) {
  //    echosub.add(tup);
  //    return;
  //  }
  //  if (!checkSubscriptionOnce) {
  //    checkSubscriptionOnce = true;
  //    if (subscription.get() == null) {
  //      errors.add(new IllegalStateException("onSubscribe not called in proper
  //      order"));
  //    }
  //  }
  //  lastThread = Thread.currentThread();

  {
    std::unique_lock<std::mutex> lock(mutex_);
    onNextValues_.push_back(std::move(element));
    ++onNextItemsCount_;
  }
  onNextValuesCV_.notify_one();

  //  numOnNext.countDown();
  //  takeLatch.countDown();

  //  actual.onNext(new PayloadImpl(tup.getK(), tup.getV()));
}

void TestSubscriber::onComplete() {
  LOG(INFO) << "onComplete";
  //  isComplete = true;
  //  if (!checkSubscriptionOnce) {
  //    checkSubscriptionOnce = true;
  //    if (subscription.get() == null) {
  //      errors.add(new IllegalStateException("onSubscribe not called in proper
  //      order"));
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

  {
    std::unique_lock<std::mutex> lock(mutex_);
    completed_ = true;
  }

  terminatedCV_.notify_one();
}

void TestSubscriber::onError(folly::exception_wrapper ex) {
  LOG(INFO) << "onError";
  //  if (!checkSubscriptionOnce) {
  //    checkSubscriptionOnce = true;
  //    if (subscription.get() == null) {
  //      errors.add(new NullPointerException("onSubscribe not called in proper
  //      order"));
  //    }
  //  }
  //  try {
  //    lastThread = Thread.currentThread();

  {
    std::unique_lock<std::mutex> lock(mutex_);
    errors_.push_back(std::move(ex));

    errored_ = true;
  }

  terminatedCV_.notify_one();

  //
  //    if (t == null) {
  //      errors.add(new IllegalStateException("onError received a null
  //      Subscription"));
  //    }
  //
  //    actual.onError(t);
  //  } finally {
  //          done.countDown();
  //  }
}

} // tck
} // reactive socket

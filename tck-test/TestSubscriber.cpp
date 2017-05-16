// Copyright 2004-present Facebook. All Rights Reserved.

#include "tck-test/TestSubscriber.h"

#include <thread>

#include <folly/Format.h>

using namespace folly;

namespace reactivesocket {
namespace tck {

TestSubscriber::TestSubscriber(int initialRequestN)
    : initialRequestN_(initialRequestN) {}

void TestSubscriber::request(int n) {
  LOG(INFO) << "... requesting " << n;
  subscription_->request(n);
}

void TestSubscriber::cancel() {
  LOG(INFO) << "... canceling ";
  canceled_ = true;
  if (auto subscription = std::move(subscription_)) {
    subscription->cancel();
  }
}

void TestSubscriber::awaitTerminalEvent() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!terminatedCV_.wait_for(lock, std::chrono::seconds(5), [&] {
        return completed_ || errored_;
      })) {
    throw std::runtime_error("Timed out while waiting for terminating event");
  }
}

void TestSubscriber::awaitAtLeast(int numItems) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!valuesCV_.wait_for(lock, std::chrono::seconds(5), [&] {
        return valuesCount_ >= numItems;
      })) {
    throw std::runtime_error("Timed out while waiting for items");
  }
}

void TestSubscriber::awaitNoEvents(int waitTime) {
  int valuesCount = valuesCount_;
  bool completed = completed_;
  bool errored = errored_;
  /* sleep override */
  std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
  if (valuesCount != valuesCount_ || completed != completed_ ||
      errored != errored_) {
    throw std::runtime_error(
        folly::sformat("Events occured within {}ms", waitTime));
  }
}

void TestSubscriber::assertNoErrors() {
  if (errored_) {
    throw std::runtime_error("Subscription completed with unexpected errors");
  }
}

void TestSubscriber::assertError() {
  if (!errored_) {
    throw std::runtime_error("Subscriber did not receive onError");
  }
}

void TestSubscriber::assertValues(
    const std::vector<std::pair<std::string, std::string>>& values) {
  assertValueCount(values.size());
  std::unique_lock<std::mutex> lock(mutex_);
  for (size_t i = 0; i < values.size(); i++) {
    if (values_[i] != values[i]) {
      throw std::runtime_error(folly::sformat(
          "Unexpected element {}:{}.  Expected element {}:{}",
          values_[i].first,
          values_[i].second,
          values[i].first,
          values[i].second));
    }
  }
}

void TestSubscriber::assertValueCount(size_t valueCount) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (values_.size() != valueCount) {
    throw std::runtime_error(folly::sformat(
        "Did not receive expected number of values! Expected={} Actual={}",
        valueCount,
        values_.size()));
  }
}

void TestSubscriber::assertReceivedAtLeast(size_t valueCount) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (values_.size() < valueCount) {
    throw std::runtime_error(folly::sformat(
        "Did not receive the minimum number of values! Expected={} Actual={}",
        valueCount,
        values_.size()));
  }
}

void TestSubscriber::assertCompleted() {
  if (!completed_) {
    throw std::runtime_error("Subscriber did not completed");
  }
}

void TestSubscriber::assertNotCompleted() {
  if (completed_) {
    throw std::runtime_error("Subscriber unexpectedly completed");
  }
}

void TestSubscriber::assertCanceled() {
  if (!canceled_) {
    throw std::runtime_error("Subscription should be canceled");
  }
}

void TestSubscriber::assertTerminated() {
  if (!completed_ && !errored_) {
    throw std::runtime_error(
        "Subscription is not terminated yet. "
        "This is most likely a bug in the test.");
  }
}

void TestSubscriber::onSubscribe(
    yarpl::Reference<yarpl::flowable::Subscription> subscription) noexcept {
  VLOG(4) << "OnSubscribe in TestSubscriber";
  subscription_ = subscription;
  if (initialRequestN_ > 0) {
    subscription_->request(initialRequestN_);
  }
}

void TestSubscriber::onNext(Payload element) noexcept {
  LOG(INFO) << "... received onNext from Publisher: " << element;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    std::string data =
        element.data ? element.data->moveToFbString().toStdString() : "";
    std::string metadata = element.metadata
        ? element.metadata->moveToFbString().toStdString()
        : "";
    values_.push_back(std::make_pair(data, metadata));
    ++valuesCount_;
  }
  valuesCV_.notify_one();
}

void TestSubscriber::onComplete() noexcept {
  LOG(INFO) << "... received onComplete from Publisher";
  {
    std::unique_lock<std::mutex> lock(mutex_);
    completed_ = true;
  }

  terminatedCV_.notify_one();
}

void TestSubscriber::onError(std::exception_ptr ex) noexcept {
  LOG(INFO) << "... received onError from Publisher";
  {
    std::unique_lock<std::mutex> lock(mutex_);
    errors_.push_back(std::move(ex));
    errored_ = true;
  }
  terminatedCV_.notify_one();
}

} // tck
} // reactivesocket

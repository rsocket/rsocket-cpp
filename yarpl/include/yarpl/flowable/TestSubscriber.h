// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <condition_variable>
#include <mutex>
#include <sstream>
#include <vector>

#include "yarpl/flowable/Flowable.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/utils/ExceptionString.h"
#include "yarpl/utils/credits.h"

namespace yarpl {
namespace flowable {

/**
 * A utility class for unit testing or experimenting with Flowable.
 *
 * Example usage:
 *
 * auto flowable = ...
 * auto ts = TestSubscriber<int>::create();
 * flowable->subscribe(to);
 * ts->awaitTerminalEvent();
 * ts->assert...
 */
template <typename T>
class TestSubscriber : public Subscriber<T> {
 public:
  static_assert(
      std::is_copy_constructible<T>::value,
      "Requires copyable types in case of a delegate subscriber");

  constexpr static auto kCanceled = credits::kCanceled;
  constexpr static auto kNoFlowControl = credits::kNoFlowControl;

  /**
   * Create a TestSubscriber that will subscribe and store the value it
   * receives.
   */
  static Reference<TestSubscriber<T>> create(int64_t initial = kNoFlowControl) {
    return make_ref<TestSubscriber<T>>(initial);
  }

  /**
   * Create a TestSubscriber that will delegate all on* method calls
   * to the provided Subscriber.
   *
   * This will store the value it receives to allow assertions.
   */
  static Reference<TestSubscriber<T>> create(
      Reference<Subscriber<T>> delegate,
      int64_t initial = kNoFlowControl) {
    return make_ref<TestSubscriber<T>>(std::move(delegate), initial);
  }

  explicit TestSubscriber(int64_t initial = kNoFlowControl)
      : TestSubscriber(Reference<Subscriber<T>>{}, initial) {}

  explicit TestSubscriber(
      Reference<Subscriber<T>> delegate,
      int64_t initial = kNoFlowControl)
      : delegate_(std::move(delegate)), initial_{initial} {}

  void onSubscribe(Reference<Subscription> subscription) override {
    if (delegate_) {
      subscription_ = subscription; // copy
      delegate_->onSubscribe(std::move(subscription));
    } else {
      subscription_ = std::move(subscription);
    }
    subscription_->request(initial_);
  }

  void onNext(T t) override {
    if (delegate_) {
      values_.push_back(t);
      delegate_->onNext(std::move(t));
    } else {
      values_.push_back(std::move(t));
    }
  }

  void onComplete() override {
    if (delegate_) {
      delegate_->onComplete();
    }
    subscription_.reset();
    terminated_ = true;
    terminalEventCV_.notify_all();
  }

  void onError(std::exception_ptr ex) override {
    if (delegate_) {
      delegate_->onError(ex);
    }
    e_ = ex;
    subscription_.reset();
    terminated_ = true;
    terminalEventCV_.notify_all();
  }

  /**
   * Block the current thread until either onSuccess or onError is called.
   */
  void awaitTerminalEvent() {
    // now block this thread
    std::unique_lock<std::mutex> lk(m_);
    // if shutdown gets implemented this would then be released by it
    terminalEventCV_.wait(lk, [this] { return terminated_; });
  }

  void assertValueCount(size_t count) {
    if (values_.size() != count) {
      std::stringstream ss;
      ss << "Value count " << values_.size() << " does not match " << count;
      throw std::runtime_error(ss.str());
    }
  }

  int64_t getValueCount() {
    return values_.size();
  }

  std::vector<T>& values() {
    return values_;
  }

  const std::vector<T>& values() const {
    return values_;
  }

  bool isComplete() const {
    return terminated_ && !e_;
  }

  bool isError() const {
    return terminated_ && e_;
  }

  std::string getErrorMsg() const {
    if (e_ == nullptr) {
      return "";
    }
    return exceptionStr(e_);
  }

  void assertValueAt(int64_t index, T expected) {
    if (index < getValueCount()) {
      auto& v = values_[index];
      if (expected != v) {
        std::stringstream ss;
        ss << "Expected: " << expected << " Actual: " << v;
        throw std::runtime_error(ss.str());
      }
    } else {
      std::stringstream ss;
      ss << "Index " << index << " is larger than received values "
         << values_.size();
      throw std::runtime_error(ss.str());
    }
  }

  /**
   * If an onComplete call was not received throw a runtime_error
   */
  void assertSuccess() {
    if (!terminated_) {
      throw std::runtime_error("Did not receive terminal event.");
    }
    if (e_) {
      throw std::runtime_error("Received onError instead of onSuccess");
    }
  }

  /**
   * If the onError exception_ptr points to an error containing
   * the given msg, complete successfully, otherwise throw a runtime_error
   */
  void assertOnErrorMessage(std::string msg) {
    if (e_ == nullptr) {
      std::stringstream ss;
      ss << "exception_ptr == nullptr, but expected " << msg;
      throw std::runtime_error(ss.str());
    }
    try {
      std::rethrow_exception(e_);
    } catch (std::runtime_error& re) {
      if (re.what() != msg) {
        std::stringstream ss;
        ss << "Error message is: " << re.what() << " but expected: " << msg;
        throw std::runtime_error(ss.str());
      }
    } catch (...) {
      throw std::runtime_error("Expects an std::runtime_error");
    }
  }

  /**
   * Submit Subscription->cancel();
   */
  void cancel() {
    subscription_->cancel();
  }

 private:
  Reference<Subscriber<T>> delegate_;
  std::vector<T> values_;
  std::exception_ptr e_;
  int64_t initial_{kNoFlowControl};
  bool terminated_{false};
  std::mutex m_;
  std::condition_variable terminalEventCV_;
  Reference<Subscription> subscription_;
};
}
}

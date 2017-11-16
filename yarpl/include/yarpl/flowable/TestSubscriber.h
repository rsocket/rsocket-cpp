// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <condition_variable>
#include <mutex>
#include <sstream>
#include <vector>

#include "yarpl/flowable/Flowable.h"
#include "yarpl/flowable/Subscriber.h"
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
class TestSubscriber :
  public BaseSubscriber<T>,
  public yarpl::flowable::Subscription
{
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

  void onSubscribeImpl() override {
    if (delegate_) {
      delegate_->onSubscribe(this->ref_from_this(this));
    }
    this->request(initial_);
  }

  void onNextImpl(T t) override final {
    manuallyPush(std::move(t));
  }

  void manuallyPush(T t) {
    if (dropValues_) {
      valueCount_++;
    } else {
      if (delegate_) {
        values_.push_back(t);
        delegate_->onNext(std::move(t));
      } else {
        values_.push_back(std::move(t));
      }
    }

    terminalEventCV_.notify_all();
  }

  void onCompleteImpl() override final {
    if (delegate_) {
      delegate_->onComplete();
    }
    terminated_ = true;
    terminalEventCV_.notify_all();
  }

  void onErrorImpl(folly::exception_wrapper ex) override final {
    if (delegate_) {
      delegate_->onError(ex);
    }
    e_ = std::move(ex);
    terminated_ = true;
    terminalEventCV_.notify_all();
  }

  // flowable::Subscription methods
  void request(int64_t n) override {
    this->BaseSubscriber<T>::request(n);
  }
  void cancel() override {
    this->BaseSubscriber<T>::cancel();
  }

  /**
   * Block the current thread until either onSuccess or onError is called.
   */
  void awaitTerminalEvent(
      std::chrono::milliseconds ms = std::chrono::seconds{1}) {
    // now block this thread
    std::unique_lock<std::mutex> lk(m_);
    // if shutdown gets implemented this would then be released by it
    if (!terminalEventCV_.wait_for(lk, ms, [this] { return terminated_; })) {
      throw std::runtime_error("timeout in awaitTerminalEvent");
    }
  }

  void awaitValueCount(
      int64_t n,
      std::chrono::milliseconds ms = std::chrono::seconds{1}) {
    // now block this thread
    std::unique_lock<std::mutex> lk(m_);

    auto didTimeOut = terminalEventCV_.wait_for(lk, ms, [this, n] {
      if (getValueCount() < n && terminated_) {
        std::stringstream msg;
        msg << "onComplete/onError called before valueCount() == n;\nvalueCount: "
            << getValueCount() << " != " << n;
        throw std::runtime_error(msg.str());
      }
      return getValueCount() >= n;
    });

    if (!didTimeOut) {
      throw std::runtime_error("timeout in awaitValueCount");
    };
  }

  void assertValueCount(size_t count) {
    if (values_.size() != count) {
      std::stringstream ss;
      ss << "Value count " << values_.size() << " does not match " << count;
      throw std::runtime_error(ss.str());
    }
  }

  int64_t getValueCount() {
    if (dropValues_) {
      return valueCount_;
    } else {
      return values_.size();
    }
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
    return e_ ? e_.get_exception()->what() : "";
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
   * If the onError exception_wrapper points to an error containing
   * the given msg, complete successfully, otherwise throw a runtime_error
   */
  void assertOnErrorMessage(std::string msg) {
    if (!e_ || e_.get_exception()->what() != msg) {
      std::stringstream ss;
      ss << "Error is: '" << e_ << "' but expected: '" << msg << "'";
      throw std::runtime_error(ss.str());
    }
  }

  void dropValues(bool drop) {
    valueCount_ = getValueCount();
    dropValues_ = drop;
  }

 private:
  bool dropValues_{false};
  std::atomic<int> valueCount_{0};

  Reference<Subscriber<T>> delegate_;
  std::vector<T> values_;
  folly::exception_wrapper e_;
  int64_t initial_{kNoFlowControl};
  bool terminated_{false};
  std::mutex m_;
  std::condition_variable terminalEventCV_;
  Reference<Subscription> subscription_;
};
} // namespace flowable
} // namespace yarpl

// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <vector>
#include "Single.h"
#include "SingleObserver.h"

namespace yarpl {
namespace single {

/**
 * A utility class for unit testing or experimenting with Single.
 *
 * Example usage:
 *
 * auto single = ...
 * auto to = SingleTestObserver<int>::create();
 * single->subscribe(to);
 * ts->awaitTerminalEvent();
 * ts->assert...
 *
 * If you have a SingleObserver impl with specific logic you want used,
 * you can pass it into the SingleTestObserver and the on* events will be
 * delegated to your implementation.
 *
 * For example:
 *
 * auto to = SingleTestObserver<int>::create(make_ref<MyObserver>());
 * single->subscribe(to);
 *
 * Now when 'single' is subscribed to, the SingleTestObserver behavior
 * will be used, but 'MyObserver' on* methods will also be invoked.
 *
 * @tparam T
 */
template <typename T>
class SingleTestObserver : public yarpl::single::SingleObserver<T> {
 public:
  /**
   * Create a SingleTestObserver that will subscribe and store the value it
   * receives.
   *
   * @return
   */
  static Reference<SingleTestObserver<T>> create() {
    return make_ref<SingleTestObserver<T>>();
  }

  /**
   * Create a SingleTestObserver that will delegate all on* method calls
   * to the provided SingleObserver.
   *
   * This will store the value it receives to allow assertions.
   * @return
   */
  static Reference<SingleTestObserver<T>> create(
      Reference<SingleObserver<T>> delegate) {
    return make_ref<SingleTestObserver<T>>(std::move(delegate));
  }

  SingleTestObserver() : delegate_(nullptr) {}

  // TODO thread safety
  // generally an observer assumes single threaded emission
  // but this class is intended for use in unit tests
  // when it will generally receive events on one thread
  // and then access them for verification/assertion
  // on the unit test main thread

  explicit SingleTestObserver(Reference<SingleObserver<T>> delegate)
      : delegate_(std::move(delegate)) {}

  void onSubscribe(Reference<SingleSubscription> subscription) override {
    LOG(INFO) << "received SingleSubscription";
    if (delegate_) {
      delegateSubscription_->setDelegate(subscription); // copy
      delegate_->onSubscribe(std::move(subscription));
    } else {
      delegateSubscription_->setDelegate(std::move(subscription));
    }
    LOG(INFO) << "AFTER received SingleSubscription";
  }

  void onSuccess(T t) override {
    // TODO protect with mutex but do NOT hold while emitting
    if (delegate_) {
      value_ = t; // take copy
      delegate_->onSuccess(std::move(t));
    } else {
      value_ = std::move(t);
    }
    delegateSubscription_ = nullptr;
    terminated_ = true;
    terminalEventCV_.notify_all();
  }

  void onError(const std::exception_ptr ex) override {
    if (delegate_) {
      // Do NOT hold the mutex while emitting
      delegate_->onError(ex);
    }
    // TODO protect with mutex
    e_ = ex;
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

  /**
   * Assert no onSuccess or onError events were received
   */
  void assertNoTerminalEvent() {
    // TODO protect with mutex
    if (terminated_) {
      throw std::runtime_error("An unexpected terminal event was received.");
    }
  }
  /**
   * If an onSuccess call was not received throw a runtime_error
   */
  void assertSuccess() {
    // TODO protect with mutex
    if (!terminated_) {
      throw std::runtime_error("Did not receive terminal event.");
    }
    if (e_) {
      throw std::runtime_error("Received onError instead of onSuccess");
    }
  }

  void assertOnSuccessValue(T t) {
    // TODO protect with mutex
    assertSuccess();
    if (value_ != t) {
      std::stringstream ss;
      ss << "value == " << value_ << ", but expected " << t;
      throw std::runtime_error(ss.str());
    }
  }

  /**
   * Get a reference to the received value if onSuccess was called.
   *
   * @return
   */
  T& getOnSuccessValue() {
    // TODO protect with mutex
    return value_;
  }

  /**
   * If the onError exception_ptr points to an error containing
   * the given msg, complete successfully, otherwise throw a runtime_error
   */
  void assertOnErrorMessage(std::string msg) {
    // TODO protect with mutex
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
   * Submit SingleSubscription->cancel();
   */
  void cancel() {
    LOG(INFO) << "attempt cancelling SingleSubscription";
    delegateSubscription_->cancel();
  }

 private:
  std::mutex m_;
  std::condition_variable terminalEventCV_;
  Reference<SingleObserver<T>> delegate_;
  // The following variables must be protected by mutex m_
  T value_;
  std::exception_ptr e_;
  bool terminated_{false};
  // allows thread-safe cancellation against a delegate
  // regardless of when it is received
  Reference<DelegateSingleSubscription> delegateSubscription_{
      make_ref<DelegateSingleSubscription>()};
};
}
}

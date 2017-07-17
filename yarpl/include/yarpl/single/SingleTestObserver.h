// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <vector>

#include "yarpl/single/Single.h"
#include "yarpl/single/SingleObserver.h"
#include "yarpl/single/SingleSubscriptions.h"

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

  // Note on thread safety =>
  // Generally an observer assumes single threaded emission
  // but this class is intended for use in unit tests
  // when it will generally receive events on one thread
  // and then access them for verification/assertion
  // on the unit test main thread.

  explicit SingleTestObserver(Reference<SingleObserver<T>> delegate)
      : delegate_(std::move(delegate)) {}

  void onSubscribe(Reference<SingleSubscription> subscription) override {
    if (delegate_) {
      delegateSubscription_->setDelegate(subscription); // copy
      delegate_->onSubscribe(std::move(subscription));
    } else {
      delegateSubscription_->setDelegate(std::move(subscription));
    }
  }

  void onSuccess(T t) override {
    {
      // take lock with local scope so we can emit without holding the lock
      std::lock_guard<std::mutex> g(m_);
      if (delegate_) {
        value_ = t; // take copy
        // do not emit here, but later without lock
      } else {
        value_ = std::move(t);
      }
      delegateSubscription_ = nullptr;
      terminated_ = true;
    }
    // after lock is released we emit
    if (delegate_) {
      // Do NOT hold the mutex while emitting
      delegate_->onSuccess(std::move(t));
    }
    // then we notify that we're completed
    terminalEventCV_.notify_all();
  }

  void onError(std::exception_ptr ex) override {
    if (delegate_) {
      // Do NOT hold the mutex while emitting
      delegate_->onError(ex);
    }
    {
      std::lock_guard<std::mutex> g(m_);
      e_ = ex;
      terminated_ = true;
    }
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
    std::lock_guard<std::mutex> g(m_);
    if (terminated_) {
      throw std::runtime_error("An unexpected terminal event was received.");
    }
  }
  /**
   * If an onSuccess call was not received throw a runtime_error
   */
  void assertSuccess() {
    std::lock_guard<std::mutex> g(m_);
    if (!terminated_) {
      throw std::runtime_error("Did not receive terminal event.");
    }
    if (e_) {
      throw std::runtime_error("Received onError instead of onSuccess");
    }
  }

  void assertOnSuccessValue(T t) {
    assertSuccess();
    std::lock_guard<std::mutex> g(m_);
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
    std::lock_guard<std::mutex> g(m_);
    return value_;
  }

  /**
   * If the onError exception_ptr points to an error containing
   * the given msg, complete successfully, otherwise throw a runtime_error
   */
  void assertOnErrorMessage(std::string msg) {
    std::lock_guard<std::mutex> g(m_);
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
    // do NOT hold a lock while invoking the normal signals
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

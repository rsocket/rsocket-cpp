// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <condition_variable>
#include <mutex>
#include <sstream>
#include <vector>

#include "yarpl/Observable.h"

namespace yarpl {
namespace observable {

/**
 * A utility class for unit testing or experimenting with Observable.
 *
 * Example usage:
 *
 * auto observable = ...
 * auto ts = TestObserver<int>::create();
 * observable->subscribe(ts->unique_observer());
 * ts->awaitTerminalEvent();
 * ts->assert...
 *
 * If you have an Observer impl with specific logic you want used,
 * you can pass it into the TestObserver and the on* events will be
 * delegated to your implementation.
 *
 * For example:
 *
 * auto ts = TestObserver<int>::create(std::make_unique<MyObserver>());
 * observable->subscribe(ts->unique_observer());
 *
 * Now when 'observable' is subscribed to, the TestObserver behavior
 * will be used, but 'MyObserver' on* methods will also be invoked.
 *
 * @tparam T
 */
template <typename T>
class TestObserver : public yarpl::observable::Observer<T>,
                     public std::enable_shared_from_this<TestObserver<T>> {
  using Subscription = yarpl::observable::Subscription;
  using Observer = yarpl::observable::Observer<T>;

 public:
  /**
   * Create a TestObserver that will subscribe upwards
   * with no flow control (max value) and store all values it receives.
   * @return
   */
  static std::shared_ptr<TestObserver<T>> create();

  /**
   * Create a TestObserver that will delegate all on* method calls
   * to the provided Observer.
   *
   * This will store all values it receives to allow assertions.
   * @return
   */
  static std::shared_ptr<TestObserver<T>> create(std::unique_ptr<Observer>);

  TestObserver();
  explicit TestObserver(std::unique_ptr<Observer> delegate);

  void onSubscribe(Subscription* s) override;
  void onNext(const T& t) override;
  void onComplete() override;
  void onError(std::exception_ptr ex) override;

  /**
   * Get a unique Observer<T> that can be passed into the Observable.subscribe
   * method which requires a unique_ptr<Observer>.
   *
   * This decouples the lifetime of TestObserver from what is passed into
   * the Observable.subscribe method so that the testing code can retain
   * a reference to TestObserver to use it beyond the lifecycle
   * of Observable.subscribe.
   *
   * @return
   */
  std::unique_ptr<yarpl::observable::Observer<T>> unique_observer();

  /**
   * Block the current thread until either onComplete or onError is called.
   */
  void awaitTerminalEvent();

  /**
   * If the onNext values received does not match the given count,
   * throw a runtime_error
   */
  void assertValueCount(size_t count);

  /**
   * The number of onNext values received.
   * @return
   */
  int64_t getValueCount();

  /**
   * Get a reference to a stored value at a given index position.
   *
   * The values are stored in the order received from onNext.
   */
  T& getValueAt(size_t index);

  /**
   * If the onError exception_ptr points to an error containing
   * the given msg, complete successfully, otherwise throw a runtime_error
   */
  void assertOnErrorMessage(std::string msg);

  /**
   * Submit Subscription->cancel();
   */
  void cancel();

 private:
  std::unique_ptr<Observer> delegate_;
  std::vector<T> values_;
  std::exception_ptr e_;
  bool terminated_{false};
  std::mutex m_;
  std::condition_variable terminalEventCV_;
  Subscription* subscription_;
};

template <typename T>
TestObserver<T>::TestObserver() : delegate_(nullptr){};

template <typename T>
TestObserver<T>::TestObserver(std::unique_ptr<Observer> delegate)
    : delegate_(std::move(delegate)){};

template <typename T>
std::shared_ptr<TestObserver<T>> TestObserver<T>::create() {
  return std::make_shared<TestObserver<T>>();
}

template <typename T>
std::shared_ptr<TestObserver<T>> TestObserver<T>::create(
    std::unique_ptr<Observer> s) {
  return std::make_shared<TestObserver<T>>(std::move(s));
}

template <typename T>
void TestObserver<T>::onSubscribe(Subscription* s) {
  subscription_ = s;
  if (delegate_) {
    delegate_->onSubscribe(s);
  }
}

template <typename T>
void TestObserver<T>::onNext(const T& t) {
  if (delegate_) {
    //    std::cout << "TestObserver onNext& => copy then delegate" <<
    //    std::endl;
    values_.push_back(t);
    delegate_->onNext(t);
  } else {
    //    std::cout << "TestObserver onNext& => copy" << std::endl;
    values_.push_back(t);
  }
}

template <typename T>
void TestObserver<T>::onComplete() {
  if (delegate_) {
    delegate_->onComplete();
  }
  terminated_ = true;
  terminalEventCV_.notify_all();
}

template <typename T>
void TestObserver<T>::onError(std::exception_ptr ex) {
  if (delegate_) {
    delegate_->onError(ex);
  }
  e_ = ex;
  terminated_ = true;
  terminalEventCV_.notify_all();
}

template <typename T>
void TestObserver<T>::awaitTerminalEvent() {
  // now block this thread
  std::unique_lock<std::mutex> lk(m_);
  // if shutdown gets implemented this would then be released by it
  terminalEventCV_.wait(lk, [this] { return terminated_; });
}

template <typename T>
void TestObserver<T>::cancel() {
  subscription_->cancel();
}

template <typename T>
std::unique_ptr<yarpl::observable::Observer<T>>
TestObserver<T>::unique_observer() {
  class UObserver : public yarpl::observable::Observer<T> {
   public:
    UObserver(std::shared_ptr<TestObserver<T>> ts) : ts_(std::move(ts)) {}

    void onSubscribe(yarpl::observable::Subscription* s) override {
      ts_->onSubscribe(s);
    }

    void onNext(const T& t) override {
      ts_->onNext(t);
    }

    void onError(std::exception_ptr e) override {
      ts_->onError(e);
    }

    void onComplete() override {
      ts_->onComplete();
    }

   private:
    std::shared_ptr<TestObserver<T>> ts_;
  };

  return std::make_unique<UObserver>(this->shared_from_this());
}

template <typename T>
void TestObserver<T>::assertValueCount(size_t count) {
  if (values_.size() != count) {
    std::stringstream ss;
    ss << "Value count " << values_.size() << " does not match " << count;
    throw std::runtime_error(ss.str());
  }
}
template <typename T>
int64_t TestObserver<T>::getValueCount() {
  return values_.size();
}

template <typename T>
T& TestObserver<T>::getValueAt(size_t index) {
  return values_[index];
}

template <typename T>
void TestObserver<T>::assertOnErrorMessage(std::string msg) {
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
}
}

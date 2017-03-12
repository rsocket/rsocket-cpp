#pragma once

#include <folly/Baton.h>
#include "reactivestreams/ReactiveStreams.h"

namespace yarpl {
namespace flowable {

/**
 * A utility class for unit testing or experimenting with Flowables.
 *
 * Example usage:
 *
 * auto flowable = ...
 * auto ts = TestSubscriber<int>::create();
 * flowable->subscribe(ts->unique_subscriber());
 * ts->awaitTerminalEvent();
 * ts->assert...
 *
 * If you have a Subscriber impl with specific logic you want used,
 * you can pass it into the TestSubscriber and the on* events will be
 * delegated to your implementation.
 *
 * For example:
 *
 * auto ts = TestSubscriber<int>::create(std::make_unique<MySubscriber>());
 * flowable->subscribe(ts->unique_subscriber());
 *
 * Now when 'flowable' is subscribed to, the TestSubscriber behavior
 * will be used, but 'MySubscriber' on* methods will also be invoked.
 *
 * @tparam T
 */
template <typename T>
class TestSubscriber : public reactivestreams_yarpl::Subscriber<T>,
                       public std::enable_shared_from_this<TestSubscriber<T>> {
  using Subscription = reactivestreams_yarpl::Subscription<T>;
  using Subscriber = reactivestreams_yarpl::Subscriber<T>;

 public:
  /**
   * Create a TestSubscriber that will subscribe upwards
   * with no flow control (max value) and store all values it receives.
   * @return
   */
  static std::shared_ptr<TestSubscriber<T>> create();

  /**
   * Create a TestSubscriber that will subscribe upwards
   * with the initial requestN value and store all values it receives.
   * @param n
   * @return
   */
  static std::shared_ptr<TestSubscriber<T>> create(long initialRequestN);

  /**
   * Create a TestSubscriber that will delegate all on* method calls
   * to the provided Subscriber.
   *
   * This will store all values it receives to allow assertions.
   * @return
   */
  static std::shared_ptr<TestSubscriber<T>> create(std::unique_ptr<Subscriber>);

  TestSubscriber(std::unique_ptr<Subscriber> delegate);
  TestSubscriber(long initialRequestN);

  void onSubscribe(Subscription* s) override;
  void onNext(const T& t) override;
  void onNext(T&& t) override;
  void onComplete() override;
  void onError(const std::exception_ptr ex) override;

  /**
   * Get a unique Subscriber<T> that can be passed into the Flowable.subscribe
   * method which requires a unique_ptr<Subscriber>.
   *
   * This decouples the lifetime of TestSubscriber from what is passed into
   * the Flowable.subscribe method so that the testing code can retain
   * a reference to TestSubscriber to use it beyond the lifecycle
   * of Flowable.subscribe.
   *
   * @return
   */
  std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> unique_subscriber();

  /**
   * Block the current thread until either onComplete or onError is called.
   */
  void awaitTerminalEvent();

  /**
   * If the onNext values received does not match the given count,
   * throw a runtime_error
   *
   * @param count
   */
  void assertValueCount(size_t count);

 private:
  long initialRequestN_;
  std::unique_ptr<Subscriber> delegate_;
  std::vector<T> values_;
  bool terminated_{false};
  std::mutex m_;
  std::condition_variable terminalEventCV_;
};

template <typename T>
TestSubscriber<T>::TestSubscriber(std::unique_ptr<Subscriber> delegate)
    : delegate_(std::move(delegate)){};

template <typename T>
TestSubscriber<T>::TestSubscriber(long initialRequestN)
    : initialRequestN_(initialRequestN), delegate_(nullptr){};

template <typename T>
std::shared_ptr<TestSubscriber<T>> TestSubscriber<T>::create() {
  return std::make_shared<TestSubscriber<T>>(LONG_MAX);
}

template <typename T>
std::shared_ptr<TestSubscriber<T>> TestSubscriber<T>::create(
    long initialRequestN) {
  return std::make_shared<TestSubscriber<T>>(initialRequestN);
}

template <typename T>
std::shared_ptr<TestSubscriber<T>> TestSubscriber<T>::create(
    std::unique_ptr<Subscriber> s) {
  return std::make_shared<TestSubscriber<T>>(std::move(s));
}

template <typename T>
void TestSubscriber<T>::onSubscribe(Subscription* s) {
  if (delegate_) {
    delegate_->onSubscribe(s);
  } else {
    s->request(initialRequestN_);
  }
}

template <typename T>
void TestSubscriber<T>::onNext(const T& t) {
  std::cout << "onNext& " << t << std::endl;
  if (delegate_) {
    delegate_->onNext(t);
  }
  values_.push_back(t);
}

template <typename T>
void TestSubscriber<T>::onNext(T&& t) {
  std::cout << "onNext&& " << t << std::endl;
  if (delegate_) {
    // copy with push_back rather than emplace
    // since we pass the ref into the delegate
    values_.push_back(t);
    delegate_->onNext(std::move(t));
  } else {
    values_.emplace_back(std::move(t));
  }
}

template <typename T>
void TestSubscriber<T>::onComplete() {
  std::cout << "onComplete " << std::endl;
  if (delegate_) {
    delegate_->onComplete();
  }
  terminated_ = true;
  terminalEventCV_.notify_all();
}

template <typename T>
void TestSubscriber<T>::onError(const std::exception_ptr ex) {
  std::cout << "onError " << std::endl;
  if (delegate_) {
    delegate_->onError(ex);
  }
  terminated_ = true;
  terminalEventCV_.notify_all();
}

template <typename T>
void TestSubscriber<T>::awaitTerminalEvent() {
  std::cout << "awaiting" << std::endl;
  // now block this thread
  std::unique_lock<std::mutex> lk(m_);
  // if shutdown gets implemented this would then be released by it
  terminalEventCV_.wait(lk, [this] { return terminated_; });
}

template <typename T>
std::unique_ptr<reactivestreams_yarpl::Subscriber<T>>
TestSubscriber<T>::unique_subscriber() {
  class USubscriber : public reactivestreams_yarpl::Subscriber<T> {
   public:
    USubscriber(std::shared_ptr<TestSubscriber<T>> ts) : ts_(std::move(ts)) {}
    ~USubscriber() {
      std::cout << "USubscriber destroyed" << std::endl;
    }
    void onSubscribe(reactivestreams_yarpl::Subscription<T>* s) override {
      ts_->onSubscribe(s);
    }
    void onNext(const T& t) override {
      ts_->onNext(t);
    }
    void onNext(T&& t) override {
      ts_->onNext(t);
    }
    void onError(const std::exception_ptr e) override {
      ts_->onError(e);
    }
    void onComplete() override {
      ts_->onComplete();
    }

   private:
    std::shared_ptr<TestSubscriber<T>> ts_;
  };

  return std::make_unique<USubscriber>(this->shared_from_this());
}

template <typename T>
void TestSubscriber<T>::assertValueCount(size_t count) {
  if (values_.size() != count) {
    std::stringstream ss;
    ss << "Value count " << values_.size() << " does not match " << count;
    throw std::runtime_error(ss.str());
  }
}
}
}
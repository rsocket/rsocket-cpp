// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <exception>

#include <folly/ExceptionWrapper.h>
#include <gmock/gmock.h>

#include "src/framing/FrameProcessor.h"
#include "src/internal/Common.h"
#include "yarpl/flowable/Flowable.h"

namespace rsocket {

using namespace yarpl::flowable;

/// GoogleMock-compatible Publisher implementation for fast prototyping.
/// UnmanagedMockPublisher's lifetime MUST be managed externally.
template <typename T>
class MockFlowable : public Flowable<T> {
 public:
  MOCK_METHOD1_T(subscribe_, void(yarpl::Reference<Subscriber<T>> subscriber));

  void subscribe(yarpl::Reference<Subscriber<T>> subscriber) noexcept override {
    subscribe_(std::move(subscriber));
  }
};

/// GoogleMock-compatible Subscriber implementation for fast prototyping.
/// MockSubscriber MUST be heap-allocated, as it manages its own lifetime.
/// For the same reason putting mock instance in a smart pointer is a poor idea.
/// Can only be instanciated for CopyAssignable E type.
template <typename T>
class MockSubscriber : public Subscriber<T> {
 public:
  MOCK_METHOD1(onSubscribe_, void(yarpl::Reference<Subscription> subscription));
  MOCK_METHOD1_T(onNext_, void(const T& value));
  MOCK_METHOD0(onComplete_, void());
  MOCK_METHOD1_T(onError_, void(std::exception_ptr ex));

  explicit MockSubscriber(int64_t initial = kMaxRequestN) : initial_(initial) {}

  void onSubscribe(yarpl::Reference<Subscription> subscription) override {
    subscription_ = subscription;
    onSubscribe_(subscription);

    if (initial_ > 0) {
      subscription_->request(initial_);
    }
  }

  void onNext(T element) override {
    onNext_(element);

    --waitedFrameCount_;
    framesEventCV_.notify_one();
  }

  void onComplete() override {
    onComplete_();
    subscription_.reset();
    terminated_ = true;
    terminalEventCV_.notify_all();
  }

  void onError(std::exception_ptr ex) override {
    onError_(ex);
    terminated_ = true;
    terminalEventCV_.notify_all();
  }

  Subscription* subscription() const {
    return subscription_.operator->();
  }

  /**
   * Block the current thread until either onSuccess or onError is called.
   */
  void awaitTerminalEvent(
      std::chrono::milliseconds timeout = std::chrono::seconds(1)) {
    // now block this thread
    std::unique_lock<std::mutex> lk(m_);
    // if shutdown gets implemented this would then be released by it
    bool result = terminalEventCV_.wait_for(
        lk, timeout, [this] {
          return terminated_;
        });
    EXPECT_TRUE(result) << "Timed out";
  }

  /**
   * Block the current thread until onNext is called 'count' times.
   */
  void awaitFrames(uint64_t count,
       std::chrono::milliseconds timeout = std::chrono::seconds(1)) {
    waitedFrameCount_ += count;
    std::unique_lock<std::mutex> lk(mFrame_);
    if (waitedFrameCount_ > 0) {
      bool result = framesEventCV_.wait_for(
          lk, timeout, [this] {
            return waitedFrameCount_ <= 0;
          });
      EXPECT_TRUE(result) << "Timed out";
    }
  }

 protected:
  // As the 'subscription_' member in the parent class is private,
  // we define it here again.
  yarpl::Reference<Subscription> subscription_;

  int64_t initial_{kMaxRequestN};

  bool terminated_{false};
  mutable std::mutex m_, mFrame_;
  mutable std::condition_variable terminalEventCV_, framesEventCV_;
  mutable std::atomic<int> waitedFrameCount_{0};
};

/// GoogleMock-compatible Subscriber implementation for fast prototyping.
/// MockSubscriber MUST be heap-allocated, as it manages its own lifetime.
/// For the same reason putting mock instance in a smart pointer is a poor idea.
class MockSubscription : public Subscription {
 public:
  MOCK_METHOD1(request_, void(int64_t n));
  MOCK_METHOD0(cancel_, void());

  void request(int64_t n) override {
    if (!requested_) {
      requested_ = true;
      EXPECT_CALL(checkpoint_, Call()).Times(1);
    }

    request_(n);
  }

  void cancel() override {
    cancel_();
    checkpoint_.Call();
  }

 protected:
  bool requested_{false};
  testing::MockFunction<void()> checkpoint_;
};

} // namespace rsocket

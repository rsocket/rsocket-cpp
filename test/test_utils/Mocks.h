// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cassert>
#include <exception>

#include <gmock/gmock.h>

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
  MOCK_METHOD1_T(onNext_, void(T& value));
  MOCK_METHOD0(onComplete_, void());
  MOCK_METHOD1_T(onError_, void(std::exception_ptr ex));

  void onSubscribe(
      yarpl::Reference<Subscription> subscription) override {
    subscription_ = subscription;
    // We allow registering the same subscriber with multiple Publishers.
    EXPECT_CALL(checkpoint_, Call());
    onSubscribe_(subscription);
  }

  void onNext(T element) override {
    onNext_(element);
  }

  void onComplete() override {
    onComplete_();
    checkpoint_.Call();
    subscription_ = nullptr;
  }

  void onError(std::exception_ptr ex) override {
    onError_(ex);
    checkpoint_.Call();
    subscription_ = nullptr;
  }

  Subscription* subscription() const {
    return subscription_.get();
  }

 private:
  yarpl::Reference<Subscription> subscription_;
  testing::MockFunction<void()> checkpoint_;
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

 private:
  bool requested_{false};
  testing::MockFunction<void()> checkpoint_;
};

} // namespace rsocket

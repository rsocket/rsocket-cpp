// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cassert>
#include <exception>

#include <gmock/gmock.h>

#include "reactive-streams-cpp/ReactiveStreams.h"
#include "reactive-streams-cpp/utilities/Ownership.h"

namespace reactivestreams {

/// GoogleMock-compatible Publisher implementation for fast prototyping.
/// UnmanagedMockPublisher's lifetime MUST be managed externally.
template <typename T, typename E = std::exception_ptr>
class UnmanagedMockPublisher : public Publisher<T, E> {
 public:
  MOCK_METHOD1_T(subscribe_, void(Subscriber<T, E>* subscriber));

  void subscribe(Subscriber<T, E>& subscriber) {
    subscribe_(&subscriber);
  }
};

/// GoogleMock-compatible Subscriber implementation for fast prototyping.
/// UnmanagedMockSubscriber's lifetime MUST be managed externally.
template <typename T, typename E = std::exception_ptr>
class UnmanagedMockSubscriber : public Subscriber<T, E> {
 public:
  MOCK_METHOD1(onSubscribe_, void(Subscription* subscription));
  MOCK_METHOD1_T(onNext_, void(T& value));
  MOCK_METHOD0(onComplete_, void());
  MOCK_METHOD1_T(onError_, void(E ex));

  void onSubscribe(Subscription& subscription) override {
    onSubscribe_(&subscription);
  }

  void onNext(T element) override {
    onNext_(element);
  }

  void onComplete() override {
    onComplete_();
  }

  void onError(E ex) override {
    onError_(ex);
  }
};

/// GoogleMock-compatible Subscription implementation for fast prototyping.
/// UnmanagedMockSubscription's lifetime MUST be managed externally.
class UnmanagedMockSubscription : public Subscription {
 public:
  MOCK_METHOD1(request_, void(size_t n));
  MOCK_METHOD0(cancel_, void());

  void request(size_t n) override {
    request_(n);
  }

  void cancel() override {
    cancel_();
  }
};

/// GoogleMock-compatible Subscriber implementation for fast prototyping.
/// MockSubscriber MUST be heap-allocated, as it manages its own lifetime.
/// For the same reason putting mock instance in a smart pointer is a poor idea.
/// Can only be instanciated for CopyAssignable E type.
template <typename T, typename E = std::exception_ptr>
class MockSubscriber;

template <typename T, typename E = std::exception_ptr>
MockSubscriber<T, E>& makeMockSubscriber() {
  return *(new MockSubscriber<T, E>());
}

template <typename T, typename E>
class MockSubscriber : public Subscriber<T, E> {
 public:
  MOCK_METHOD1(onSubscribe_, void(Subscription* subscription));
  MOCK_METHOD1_T(onNext_, void(T& value));
  MOCK_METHOD0(onComplete_, void());
  MOCK_METHOD1_T(onError_, void(E ex));

  void onSubscribe(Subscription& subscription) override {
    subscription_ = &subscription;
    // We allow registering the same subscriber with multiple Publishers.
    // Otherwise, we could get rid of reference counting.
    refCount_.increment();
    onSubscribe_(&subscription);
  }

  void onNext(T element) override {
    onNext_(element);
  }

  void onComplete() override {
    auto handle = refCount_.decrementDeferred();
    onComplete_();
  }

  void onError(E ex) override {
    auto handle = refCount_.decrementDeferred();
    onError_(ex);
  }

  Subscription* subscription() const {
    return subscription_;
  }

 private:
  friend MockSubscriber<T, E>& makeMockSubscriber<T, E>();

  RefCountedDeleter<MockSubscriber<T, E>> refCount_;
  Subscription* subscription_{nullptr};

  /// Private c'tor, please use the static factory method.
  /// MockSubscriber manages its own lifetime and assumes heap-allocation.
  MockSubscriber() : refCount_(this, 0) {}
};

/// GoogleMock-compatible Subscriber implementation for fast prototyping.
/// MockSubscriber MUST be heap-allocated, as it manages its own lifetime.
/// For the same reason putting mock instance in a smart pointer is a poor idea.
class MockSubscription : public Subscription {
 public:
  MOCK_METHOD1(request_, void(size_t n));
  MOCK_METHOD0(cancel_, void());

  void request(size_t n) override {
    request_(n);
  }

  void cancel() override {
    auto handle = refCount_.decrementDeferred();
    cancel_();
  }

 private:
  friend MockSubscription& makeMockSubscription();

  RefCountedDeleter<MockSubscription> refCount_;

  /// Private c'tor, please use the static factory method.
  /// MockSubscription manages its own lifetime and assumes heap-allocation.
  MockSubscription() : refCount_(this, 1) {}
};

inline MockSubscription& makeMockSubscription() {
  return *(new MockSubscription());
}
}

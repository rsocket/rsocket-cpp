// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cassert>
#include <exception>

#include <gmock/gmock.h>

#include <reactive-streams/ReactiveStreams.h>

namespace reactivestreams {

/// GoogleMock-compatible Publisher implementation for fast prototyping.
/// UnmanagedMockPublisher's lifetime MUST be managed externally.
template <typename T, typename E = std::exception_ptr>
class MockPublisher : public Publisher<T, E> {
 public:
  MOCK_METHOD1_T(
      subscribe_,
      void(std::shared_ptr<Subscriber<T, E>> subscriber));

  void subscribe(std::shared_ptr<Subscriber<T, E>> subscriber) override {
    subscribe_(std::move(subscriber));
  }
};

/// GoogleMock-compatible Subscriber implementation for fast prototyping.
/// MockSubscriber MUST be heap-allocated, as it manages its own lifetime.
/// For the same reason putting mock instance in a smart pointer is a poor idea.
/// Can only be instanciated for CopyAssignable E type.
template <typename T, typename E = std::exception_ptr>
class MockSubscriber;

// TODO: remove
template <typename T, typename E = std::exception_ptr>
std::shared_ptr<MockSubscriber<T, E>> makeMockSubscriber() {
  return std::make_shared<MockSubscriber<T, E>>();
}

using CheckpointPtr = std::shared_ptr<testing::MockFunction<void()>>;

template <typename T, typename E>
class MockSubscriber : public Subscriber<T, E> {
  class SubscriptionShim : public Subscription {
   public:
    explicit SubscriptionShim(
        std::shared_ptr<Subscription> originalSubscription,
        CheckpointPtr checkpoint)
        : originalSubscription_(std::move(originalSubscription)),
          checkpoint_(std::move(checkpoint)) {}

    void request(size_t n) override final {
      originalSubscription_->request(n);
    }

    void cancel() override final {
      checkpoint_->Call();
      originalSubscription_->cancel();
    }

   private:
    std::shared_ptr<Subscription> originalSubscription_;
    CheckpointPtr checkpoint_;
  };

 public:
  MOCK_METHOD1(onSubscribe_, void(std::shared_ptr<Subscription> subscription));
  MOCK_METHOD1_T(onNext_, void(T& value));
  MOCK_METHOD0(onComplete_, void());
  MOCK_METHOD1_T(onError_, void(E ex));

  void onSubscribe(std::shared_ptr<Subscription> subscription) override {
    subscription_ = std::make_shared<SubscriptionShim>(
        std::move(subscription), checkpoint_);
    // We allow registering the same subscriber with multiple Publishers.
    EXPECT_CALL(*checkpoint_, Call()).Times(testing::AtLeast(1));
    onSubscribe_(subscription_);
  }

  void onNext(T element) override {
    onNext_(element);
  }

  void onComplete() override {
    checkpoint_->Call();
    onComplete_();
    subscription_ = nullptr;
  }

  void onError(E ex) override {
    checkpoint_->Call();
    onError_(ex);
    subscription_ = nullptr;
  }

  Subscription* subscription() const {
    return subscription_.get();
  }

 private:
  std::shared_ptr<SubscriptionShim> subscription_;
  CheckpointPtr checkpoint_{std::make_shared<testing::MockFunction<void()>>()};
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
    cancel_();
  }
};

// TODO: remove
inline std::shared_ptr<MockSubscription> makeMockSubscription() {
  return std::make_shared<MockSubscription>();
}
}

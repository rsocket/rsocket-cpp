// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <glog/logging.h>
#include <gmock/gmock.h>
#include <reactive-streams/ReactiveStreams.h>
#include <yarpl/flowable/Subscriber.h>
#include <yarpl/flowable/Subscription.h>
#include <cassert>
#include <exception>
#include "src/internal/ReactiveStreamsCompat.h"

namespace yarpl {
namespace flowable {

/// GoogleMock-compatible Publisher implementation for fast prototyping.
/// UnmanagedMockPublisher's lifetime MUST be managed externally.
template <typename T>
class MockPublisher : public reactivestreams::Publisher<T> {
 public:
  MockPublisher() {
    VLOG(2) << "ctor MockPublisher " << this;
  }
  ~MockPublisher() {
    VLOG(2) << "dtor MockPublisher " << this;
  }

  MOCK_METHOD1_T(
      subscribe_,
      void(yarpl::Reference<yarpl::flowable::Subscriber<T>> subscriber));

  void subscribe(yarpl::Reference<yarpl::flowable::Subscriber<T>>
                     subscriber) noexcept override {
    subscribe_(std::move(subscriber));
  }
};

/// GoogleMock-compatible Subscriber implementation for fast prototyping.
/// MockSubscriber MUST be heap-allocated, as it manages its own lifetime.
/// For the same reason putting mock instance in a smart pointer is a poor idea.
/// Can only be instanciated for CopyAssignable E type.

using CheckpointPtr = std::shared_ptr<testing::MockFunction<void()>>;

template <typename T>
class MockSubscriber : public yarpl::flowable::Subscriber<T> {
  class SubscriptionShim : public yarpl::flowable::Subscription {
   public:
    explicit SubscriptionShim(
        yarpl::Reference<yarpl::flowable::Subscription> originalSubscription,
        CheckpointPtr checkpoint)
        : originalSubscription_(std::move(originalSubscription)),
          checkpoint_(std::move(checkpoint)) {
      VLOG(2) << "ctor SubscriptionShim " << this;
    }
    ~SubscriptionShim() {
      VLOG(2) << "dtor SubscriptionShim " << this;
    }

    void request(int64_t n) noexcept override final {
      originalSubscription_->request(n);
    }

    void cancel() noexcept override final {
      checkpoint_->Call();
      originalSubscription_->cancel();
    }

   private:
    yarpl::Reference<yarpl::flowable::Subscription> originalSubscription_;
    CheckpointPtr checkpoint_;
  };

 public:
  MockSubscriber() {
    VLOG(2) << "ctor MockSubscriber " << this;
  }
  ~MockSubscriber() {
    VLOG(2) << "dtor MockSubscriber " << this;
  }

  MOCK_METHOD1(
      onSubscribe_,
      void(yarpl::Reference<yarpl::flowable::Subscription> subscription));
  MOCK_METHOD1_T(onNext_, void(T& value));
  MOCK_METHOD0(onComplete_, void());
  MOCK_METHOD1(onError_, void(const std::exception_ptr ex));

  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>
                       subscription) noexcept override {
    subscription_ =
        yarpl::make_ref<SubscriptionShim>(std::move(subscription), checkpoint_);
    // We allow registering the same subscriber with multiple Publishers.
    EXPECT_CALL(*checkpoint_, Call()).Times(testing::AtLeast(1));
    onSubscribe_(subscription_);
  }

  void onNext(T element) noexcept override {
    onNext_(element);
  }

  void onComplete() noexcept override {
    checkpoint_->Call();
    onComplete_();
    subscription_ = nullptr;
  }

  void onError(const std::exception_ptr ex) noexcept override {
    checkpoint_->Call();
    onError_(ex);
    subscription_ = nullptr;
  }

  yarpl::flowable::Subscription* subscription() const {
    return subscription_.get();
  }

 private:
  yarpl::Reference<SubscriptionShim> subscription_;
  CheckpointPtr checkpoint_{std::make_shared<testing::MockFunction<void()>>()};
};

/// GoogleMock-compatible Subscriber implementation for fast prototyping.
/// MockSubscriber MUST be heap-allocated, as it manages its own lifetime.
/// For the same reason putting mock instance in a smart pointer is a poor idea.
class MockSubscription : public yarpl::flowable::Subscription {
 public:
  MockSubscription() {
    VLOG(2) << "ctor MockSubscription " << this;
  }
  ~MockSubscription() {
    VLOG(2) << "dtor MockSubscription " << this;
  }

  MOCK_METHOD1(request_, void(int64_t n));
  MOCK_METHOD0(cancel_, void());

  void request(int64_t n) noexcept override {
    request_(n);
  }

  void cancel() noexcept override {
    cancel_();
  }
};

} // flowable
} // yarpl

namespace rsocket {

/// GoogleMock-compatible Publisher implementation for fast prototyping.
/// UnmanagedMockPublisher's lifetime MUST be managed externally.
template <typename T>
class MockPublisher : public Publisher<T> {
 public:
  MockPublisher() {
    VLOG(2) << "ctor MockPublisher " << this;
  }
  ~MockPublisher() {
    VLOG(2) << "dtor MockPublisher " << this;
  }

  MOCK_METHOD1_T(subscribe_, void(std::shared_ptr<Subscriber<T>> subscriber));

  void subscribe(std::shared_ptr<Subscriber<T>> subscriber) noexcept override {
    subscribe_(std::move(subscriber));
  }
};

/// GoogleMock-compatible Subscriber implementation for fast prototyping.
/// MockSubscriber MUST be heap-allocated, as it manages its own lifetime.
/// For the same reason putting mock instance in a smart pointer is a poor idea.
/// Can only be instanciated for CopyAssignable E type.

using CheckpointPtr = std::shared_ptr<testing::MockFunction<void()>>;

template <typename T>
class MockSubscriber : public Subscriber<T> {
  class SubscriptionShim : public Subscription {
   public:
    explicit SubscriptionShim(
        std::shared_ptr<Subscription> originalSubscription,
        CheckpointPtr checkpoint)
        : originalSubscription_(std::move(originalSubscription)),
          checkpoint_(std::move(checkpoint)) {
      VLOG(2) << "ctor SubscriptionShim " << this;
    }
    ~SubscriptionShim() {
      VLOG(2) << "dtor SubscriptionShim " << this;
    }

    void request(size_t n) noexcept override final {
      originalSubscription_->request(n);
    }

    void cancel() noexcept override final {
      checkpoint_->Call();
      originalSubscription_->cancel();
    }

   private:
    std::shared_ptr<Subscription> originalSubscription_;
    CheckpointPtr checkpoint_;
  };

 public:
  MockSubscriber() {
    VLOG(2) << "ctor MockSubscriber " << this;
  }
  ~MockSubscriber() {
    VLOG(2) << "dtor MockSubscriber " << this;
  }

  MOCK_METHOD1(onSubscribe_, void(std::shared_ptr<Subscription> subscription));
  MOCK_METHOD1_T(onNext_, void(T& value));
  MOCK_METHOD0(onComplete_, void());
  MOCK_METHOD1(onError_, void(folly::exception_wrapper ex));

  void onSubscribe(
      std::shared_ptr<Subscription> subscription) noexcept override {
    subscription_ = std::make_shared<SubscriptionShim>(
        std::move(subscription), checkpoint_);
    // We allow registering the same subscriber with multiple Publishers.
    EXPECT_CALL(*checkpoint_, Call()).Times(testing::AtLeast(1));
    onSubscribe_(subscription_);
  }

  void onNext(T element) noexcept override {
    onNext_(element);
  }

  void onComplete() noexcept override {
    checkpoint_->Call();
    onComplete_();
    subscription_ = nullptr;
  }

  void onError(folly::exception_wrapper ex) noexcept override {
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
  MockSubscription() {
    VLOG(2) << "ctor MockSubscription " << this;
  }
  ~MockSubscription() {
    VLOG(2) << "dtor MockSubscription " << this;
  }

  MOCK_METHOD1(request_, void(size_t n));
  MOCK_METHOD0(cancel_, void());

  void request(size_t n) noexcept override {
    request_(n);
  }

  void cancel() noexcept override {
    cancel_();
  }
};

} // reactivesocket

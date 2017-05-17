// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

#include <folly/ExceptionWrapper.h>

#include "yarpl/flowable/Subscriber.h"
#include "yarpl/flowable/Subscription.h"

#include "src/Payload.h"
#include "src/internal/ReactiveStreamsCompat.h"

namespace rsocket {

////////////////////////////////////////////////////////////////////////////////

class NewToOldSubscription : public yarpl::flowable::Subscription {
 public:
  explicit NewToOldSubscription(std::shared_ptr<rsocket::Subscription> inner)
      : inner_{std::move(inner)} {}
  ~NewToOldSubscription() = default;

  void request(int64_t n) override {
    if (inner_) {
      inner_->request(n);
    }
  }

  void cancel() override {
    inner_->cancel();
    inner_.reset();
  }

 private:
  std::shared_ptr<rsocket::Subscription> inner_;
};

class OldToNewSubscriber : public rsocket::Subscriber<rsocket::Payload> {
 public:
  explicit OldToNewSubscriber(
      yarpl::Reference<yarpl::flowable::Subscriber<rsocket::Payload>> inner)
      : inner_{std::move(inner)} {}

  void onSubscribe(
      std::shared_ptr<rsocket::Subscription> subscription) noexcept {
    bridge_ = yarpl::Reference<yarpl::flowable::Subscription>(
        new NewToOldSubscription(std::move(subscription)));
    inner_->onSubscribe(bridge_);
  }

  void onNext(rsocket::Payload element) noexcept {
    inner_->onNext(std::move(element));
  }

  void onComplete() noexcept {
    inner_->onComplete();

    inner_.reset();
    bridge_.reset();
  }

  void onError(folly::exception_wrapper ex) noexcept {
    inner_->onError(ex.to_exception_ptr());

    inner_.reset();
    bridge_.reset();
  }

 private:
  yarpl::Reference<yarpl::flowable::Subscriber<rsocket::Payload>> inner_;
  yarpl::Reference<yarpl::flowable::Subscription> bridge_;
};

////////////////////////////////////////////////////////////////////////////////

class OldToNewSubscription : public rsocket::Subscription {
 public:
  explicit OldToNewSubscription(
      yarpl::Reference<yarpl::flowable::Subscription> inner)
      : inner_{inner} {}

  void request(size_t n) noexcept override {
    if (inner_) {
      inner_->request(n);
    }
  }

  void cancel() noexcept override {
    if (inner_) {
      inner_->cancel();
    }
    inner_.reset();
  }

  void terminate() {
    inner_.reset();
  }

 private:
  yarpl::Reference<yarpl::flowable::Subscription> inner_{nullptr};
};

class NewToOldSubscriber
    : public yarpl::flowable::Subscriber<rsocket::Payload> {
 public:
  explicit NewToOldSubscriber(
      std::shared_ptr<rsocket::Subscriber<rsocket::Payload>> inner)
      : inner_{std::move(inner)} {}

  void onSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> subscription) override {
    bridge_ = std::make_shared<OldToNewSubscription>(subscription);
    inner_->onSubscribe(bridge_);
  }

  void onNext(rsocket::Payload payload) override {
    inner_->onNext(std::move(payload));
  }

  void onComplete() override {
    if (bridge_) {
      bridge_->terminate();
    }
    inner_->onComplete();

    inner_.reset();
    bridge_.reset();
  }

  void onError(std::exception_ptr eptr) override {
    if (bridge_) {
      bridge_->terminate();
    }

    try {
      std::rethrow_exception(eptr);
    } catch (const std::exception& e) {
      inner_->onError(folly::exception_wrapper(std::move(eptr), e));
    } catch (...) {
      inner_->onError(folly::exception_wrapper(std::current_exception()));
    }

    inner_.reset();
    bridge_.reset();
  }

 private:
  std::shared_ptr<rsocket::Subscriber<rsocket::Payload>> inner_;
  std::shared_ptr<OldToNewSubscription> bridge_;
};

class EagerSubscriberBridge
    : public yarpl::flowable::Subscriber<rsocket::Payload> {
 public:
  void onSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> subscription) noexcept {
    CHECK(!subscription_);
    subscription_ = std::move(subscription);
    if (inner_) {
      inner_->onSubscribe(subscription_);
    }
  }

  void onNext(rsocket::Payload element) noexcept {
    DCHECK(inner_);
    inner_->onNext(std::move(element));
  }

  void onComplete() noexcept {
    DCHECK(inner_);
    inner_->onComplete();

    inner_.reset();
    subscription_.reset();
  }

  void onError(std::exception_ptr ex) noexcept {
    DCHECK(inner_);
    inner_->onError(std::move(ex));

    inner_.reset();
    subscription_.reset();
  }

  void subscribe(
      yarpl::Reference<yarpl::flowable::Subscriber<rsocket::Payload>> inner) {
    CHECK(!inner_); // only one call to subscribe is supported
    CHECK(inner);
    inner_ = std::move(inner);
    if (subscription_) {
      inner_->onSubscribe(subscription_);
    }
  }

 private:
  yarpl::Reference<yarpl::flowable::Subscriber<rsocket::Payload>> inner_;
  yarpl::Reference<yarpl::flowable::Subscription> subscription_;
};
}

// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

#include <folly/ExceptionWrapper.h>

#include "yarpl/flowable/Subscriber.h"
#include "yarpl/flowable/Subscription.h"

#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace rsocket {

////////////////////////////////////////////////////////////////////////////////

class NewToOldSubscription : public yarpl::flowable::Subscription {
 public:
  explicit NewToOldSubscription(
      std::shared_ptr<reactivesocket::Subscription> inner)
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
    release();
  }

 private:
  std::shared_ptr<reactivesocket::Subscription> inner_;
};

class OldToNewSubscriber
    : public reactivesocket::Subscriber<reactivesocket::Payload> {
 public:
  explicit OldToNewSubscriber(
      yarpl::Reference<yarpl::flowable::Subscriber<reactivesocket::Payload>> inner)
      : inner_{std::move(inner)} {}

  void onSubscribe(
      std::shared_ptr<reactivesocket::Subscription> subscription) noexcept {
    bridge_ = yarpl::Reference<yarpl::flowable::Subscription>(
        new NewToOldSubscription(std::move(subscription)));
    inner_->onSubscribe(bridge_);
  }

  void onNext(reactivesocket::Payload element) noexcept {
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
  yarpl::Reference<yarpl::flowable::Subscriber<reactivesocket::Payload>> inner_;
  yarpl::Reference<yarpl::flowable::Subscription> bridge_;
};

////////////////////////////////////////////////////////////////////////////////

class OldToNewSubscription : public reactivesocket::Subscription {
 public:
  explicit OldToNewSubscription(yarpl::Reference<yarpl::flowable::Subscription> inner)
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

class NewToOldSubscriber : public yarpl::flowable::Subscriber<reactivesocket::Payload> {
 public:
  explicit NewToOldSubscriber(
      std::shared_ptr<reactivesocket::Subscriber<reactivesocket::Payload>>
          inner)
      : inner_{std::move(inner)} {}

  void onSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> subscription) override {
    bridge_ = std::make_shared<OldToNewSubscription>(subscription);
    inner_->onSubscribe(bridge_);
  }

  void onNext(reactivesocket::Payload payload) override {
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
    inner_->onError(folly::exception_wrapper(std::move(eptr)));

    inner_.reset();
    bridge_.reset();
  }

 private:
  std::shared_ptr<reactivesocket::Subscriber<reactivesocket::Payload>> inner_;
  std::shared_ptr<OldToNewSubscription> bridge_;
};
}

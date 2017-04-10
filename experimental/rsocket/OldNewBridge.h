// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

#include <folly/ExceptionWrapper.h>

#include "reactivestreams/ReactiveStreams.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace rsocket {

////////////////////////////////////////////////////////////////////////////////

class NewToOldSubscription : public reactivestreams_yarpl::Subscription {
 public:
  explicit NewToOldSubscription(
      std::shared_ptr<reactivesocket::Subscription> inner)
      : inner_{std::move(inner)} {}
  ~NewToOldSubscription() = default;

  void request(int64_t n) override {
    inner_->request(n);
  }

  void cancel() override {
    inner_->cancel();
  }

 private:
  std::shared_ptr<reactivesocket::Subscription> inner_;
};

class OldToNewSubscriber
    : public reactivesocket::Subscriber<reactivesocket::Payload> {
 public:
  explicit OldToNewSubscriber(
      std::unique_ptr<
          reactivestreams_yarpl::Subscriber<reactivesocket::Payload>> inner)
      : inner_{std::move(inner)} {}

  void onSubscribe(
      std::shared_ptr<reactivesocket::Subscription> subscription) noexcept {
    bridge_ = std::make_shared<NewToOldSubscription>(std::move(subscription));
    inner_->onSubscribe(bridge_.get());
  }

  void onNext(reactivesocket::Payload element) noexcept {
    inner_->onNext(std::move(element));
  }

  void onComplete() noexcept {
    inner_->onComplete();
  }

  void onError(folly::exception_wrapper ex) noexcept {
    inner_->onError(ex.to_exception_ptr());
  }

 private:
  std::unique_ptr<reactivestreams_yarpl::Subscriber<reactivesocket::Payload>>
      inner_;
  std::shared_ptr<NewToOldSubscription> bridge_;
};

////////////////////////////////////////////////////////////////////////////////

class OldToNewSubscription : public reactivesocket::Subscription {
 public:
  explicit OldToNewSubscription(reactivestreams_yarpl::Subscription* inner)
      : inner_{inner} {}

  void request(size_t n) noexcept override {
    if (!terminated_) {
      inner_->request(n);
    }
  }

  void cancel() noexcept override {
    if (!terminated_) {
      inner_->cancel();
    }
  }

  void terminate() {
    terminated_ = true;
  }

 private:
  reactivestreams_yarpl::Subscription* inner_{nullptr};
  bool terminated_{false};
};

class NewToOldSubscriber
    : public reactivestreams_yarpl::Subscriber<reactivesocket::Payload> {
 public:
  explicit NewToOldSubscriber(
      std::shared_ptr<reactivesocket::Subscriber<reactivesocket::Payload>>
          inner)
      : inner_{std::move(inner)} {}

  void onSubscribe(reactivestreams_yarpl::Subscription* subscription) override {
    bridge_ = std::make_shared<OldToNewSubscription>(subscription);
    inner_->onSubscribe(bridge_);
  }

  void onNext(const reactivesocket::Payload& payload) override {
    // Cloning IOBufs just shares their internal buffers, so this isn't the end
    // of the world.
    reactivesocket::Payload clone(
        payload.data->clone(), payload.metadata->clone());
    inner_->onNext(std::move(clone));
  }

  void onComplete() override {
    if (bridge_) {
      bridge_->terminate();
    }
    inner_->onComplete();
  }

  void onError(std::exception_ptr eptr) override {
    if (bridge_) {
      bridge_->terminate();
    }
    inner_->onError(folly::exception_wrapper(std::move(eptr)));
  }

 private:
  std::shared_ptr<reactivesocket::Subscriber<reactivesocket::Payload>> inner_;
  std::shared_ptr<OldToNewSubscription> bridge_;
};
}

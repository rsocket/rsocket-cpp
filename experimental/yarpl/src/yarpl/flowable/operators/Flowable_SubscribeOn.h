// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <experimental/yarpl/include/yarpl/Scheduler.h>
#include "reactivestreams/ReactiveStreams.h"

namespace yarpl {
namespace operators {

// TODO this leaks right now, need to implement 'delete this'
class SubscribeOnSubscription : public reactivestreams_yarpl::Subscription {
 public:
  SubscribeOnSubscription(
      reactivestreams_yarpl::Subscription* upstream,
      Scheduler& scheduler)
      : upstream_(upstream), worker_(scheduler.createWorker()) {
    worker_ = scheduler.createWorker();
  }
  ~SubscribeOnSubscription() {
    // TODO remove this once happy with it
    std::cout << "SubscribeOnSubscription being destroyed" << std::endl;
  }
  void request(int64_t n) override {
    auto u = upstream_;
    worker_->schedule([n, u]() { u->request(n); });
  }

  void cancel() override {
    upstream_->cancel();
  }

 private:
  reactivestreams_yarpl::Subscription* upstream_;
  std::unique_ptr<Worker> worker_;
};

template <typename T>
class SubscribeOnSubscriber : public reactivestreams_yarpl::Subscriber<T> {
 public:
  SubscribeOnSubscriber(SubscribeOnSubscriber&&) =
      default; // only allow std::move
  SubscribeOnSubscriber(const SubscribeOnSubscriber&) = delete;
  SubscribeOnSubscriber& operator=(SubscribeOnSubscriber&&) =
      default; // only allow std::move
  SubscribeOnSubscriber& operator=(const SubscribeOnSubscriber&) = delete;

  SubscribeOnSubscriber(
      std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> s,
      yarpl::Scheduler& scheduler)
      : downstream_(std::move(s)), scheduler_(scheduler) {}

  void onSubscribe(reactivestreams_yarpl::Subscription* upstreamSubscription) {
    auto sos = new SubscribeOnSubscription(upstreamSubscription, scheduler_);
    downstream_->onSubscribe(sos);
  }

  void onNext(const T& t) {
    downstream_->onNext(t);
  }

  void onNext(T&& t) {
    downstream_->onNext(t);
  }

  void onComplete() {
    downstream_->onComplete();
  }

  void onError(const std::exception_ptr error) {
    downstream_->onError(error);
  }

 private:
  std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> downstream_;
  Scheduler& scheduler_;
};

template <typename T>
class FlowableSubscribeOnOperator {
 public:
  explicit FlowableSubscribeOnOperator(yarpl::Scheduler& scheduler)
      : scheduler_(scheduler) {}
  std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> operator()(
      std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> s) {
    return std::make_unique<SubscribeOnSubscriber<T>>(std::move(s), scheduler_);
  }

 private:
  yarpl::Scheduler& scheduler_;
};

} // operators
} // yarpl

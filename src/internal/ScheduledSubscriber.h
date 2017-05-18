// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "ScheduledSubscription.h"
#include "yarpl/flowable/Subscriber.h"
#include <folly/io/async/EventBase.h>

namespace rsocket {

//
// A decorator of the Subscriber object which schedules the method calls on the
// provided EventBase
//
template<typename T>
class ScheduledSubscriber : public yarpl::flowable::Subscriber<T> {
 public:
  ScheduledSubscriber(
      yarpl::Reference<yarpl::flowable::Subscriber<T>> inner,
      folly::EventBase& eventBase) : inner_(std::move(inner)),
                                     eventBase_(eventBase) {}

  void onSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> subscription) override {
    if (eventBase_.isInEventBaseThread()) {
      inner_->onSubscribe(std::move(subscription));
    } else {
      eventBase_.runInEventBaseThread(
      [inner = inner_, subscription = std::move(subscription)]
      {
        inner->onSubscribe(std::move(subscription));
      });
    }
  }

  // No further calls to the subscription after this method is invoked.
  void onComplete() override {
    if (eventBase_.isInEventBaseThread()) {
      inner_->onComplete();
    } else {
      eventBase_.runInEventBaseThread(
      [inner = inner_]
      {
        inner->onComplete();
      });
    }
  }

  void onError(const std::exception_ptr ex) override {
    if (eventBase_.isInEventBaseThread()) {
      inner_->onError(std::move(ex));
    } else {
      eventBase_.runInEventBaseThread(
      [inner = inner_, ex = std::move(ex)]() mutable {
        inner->onError(std::move(ex));
      });
    }
  }

  void onNext(T value) override {
    if (eventBase_.isInEventBaseThread()) {
      inner_->onNext(std::move(value));
    } else {
      eventBase_.runInEventBaseThread(
      [inner = inner_, value = std::move(value)]() mutable {
        inner->onNext(std::move(value));
      });
    }
  }

 private:
  yarpl::Reference<yarpl::flowable::Subscriber<T>> inner_;
  folly::EventBase& eventBase_;
};

//
// A decorator of a Subscriber object which schedules the method calls on the
// provided EventBase
//
template<typename T>
class ScheduledSubscriptionSubscriber : public yarpl::flowable::Subscriber<T> {
 public:
  ScheduledSubscriptionSubscriber(
      yarpl::Reference<yarpl::flowable::Subscriber<T>> inner,
      folly::EventBase& eventBase) : inner_(std::move(inner)),
                                     eventBase_(eventBase) {}

  void onSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> subscription) override {
    inner_->onSubscribe(
        yarpl::make_ref<ScheduledSubscription>(subscription, eventBase_));
  }

  // No further calls to the subscription after this method is invoked.
  void onComplete() override {
    inner_->onComplete();
  }

  void onError(const std::exception_ptr ex) override {
    inner_->onError(std::move(ex));
  }

  void onNext(T value) override {
    inner_->onNext(std::move(value));
  }

 private:
  yarpl::Reference<yarpl::flowable::Subscriber<T>> inner_;
  folly::EventBase& eventBase_;
};

} // rsocket

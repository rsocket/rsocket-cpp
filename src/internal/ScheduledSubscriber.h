// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/internal/ScheduledSubscription.h"

#include <folly/io/async/EventBase.h>

#include "yarpl/flowable/Subscriber.h"

namespace rsocket {

//
// A decorator of the Subscriber object which schedules the method calls on the
// provided EventBase.
// This class should be used to wrap a Subscriber returned to the application
// code so that calls to on{Subscribe,Next,Complete,Error} are scheduled on the
// right EventBase.
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

  void onError(std::exception_ptr ex) override {
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
// provided EventBase.
// This class is to wrap the Subscriber provided from the application code to
// the library. The Subscription passed to onSubscribe method needs to be
// wrapped in the ScheduledSubscription since the application code calls
// request and cancel from any thread.
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

  void onError(std::exception_ptr ex) override {
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

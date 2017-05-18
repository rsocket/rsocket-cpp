// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/single/SingleObserver.h"
#include "yarpl/single/Singles.h"
#include <folly/io/async/EventBase.h>


namespace rsocket {

//
// A decorated RSocketResponder object which schedules the calls from
// application code to RSocket on the provided EventBase
// This class should be used to wrap a SingleObserver returned to the
// application code so that calls to on{Subscribe,Success,Error} are
// scheduled on the right EventBase.
//
template<typename T>
class ScheduledSingleObserver : public yarpl::single::SingleObserver<T> {
 public:
  ScheduledSingleObserver(
      yarpl::Reference<yarpl::single::SingleObserver<T>> observer,
      folly::EventBase& eventBase) :
      inner_(std::move(observer)), eventBase_(eventBase) {}

  void onSubscribe(
      yarpl::Reference<yarpl::single::SingleSubscription> subscription) override {
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
  void onSuccess(T value) override {
    if (eventBase_.isInEventBaseThread()) {
      inner_->onSuccess(std::move(value));
    } else {
      eventBase_.runInEventBaseThread(
      [inner = inner_, value = std::move(value)]() mutable {
        inner->onSuccess(std::move(value));
      });
    }
  }

  // No further calls to the subscription after this method is invoked.
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

 private:
  yarpl::Reference<yarpl::single::SingleObserver<T>> inner_;
  folly::EventBase& eventBase_;
};
} // rsocket

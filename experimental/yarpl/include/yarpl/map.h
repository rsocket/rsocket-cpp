// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <yarpl/flowable.h>
#include "reactivestreams/ReactiveStreams.h"

namespace yarpl {
namespace operators {

using reactivestreams_yarpl::Subscriber;

template <typename T, typename Transform>
class Mapper : public flowable::Flowable<T>,
               public reactivestreams_yarpl::Subscriber<T> {
  using Flowable = flowable::Flowable<T>;
  using Subscriber = reactivestreams_yarpl::Subscriber<T>;
  using Subscription = reactivestreams_yarpl::Subscription;

 public:
  Mapper(Transform&& transform, std::shared_ptr<Flowable> upstream)
      : forwarder_(
            std::make_unique<Forwarder>(std::forward<Transform>(transform))),
        upstream_(upstream) {}

  void subscribe(std::unique_ptr<Subscriber> subscriber) override {
    // TODO(vjn): check that subscriber_.get() == nullptr?
    forwarder_->set_subscriber(std::move(subscriber));
    upstream_->subscribe(std::move(forwarder_));
  }

 private:
  class Forwarder : public Subscriber {
   public:
    Forwarder(Transform&& transform)
        : transform_(std::forward<Transform>(transform)) {}

    void set_subscriber(std::unique_ptr<Subscriber> subscriber) {
      subscriber_ = std::move(subscriber);
    }

    void onSubscribe(Subscription* subscription) override {
      subscriber_->onSubscribe(subscription);
    }

    virtual void onComplete() override {
      subscriber_->onComplete();
    }

    void onError(const std::exception_ptr error) override {
      subscriber_->onError(error);
    }

   private:
    Transform transform_;
    std::unique_ptr<Subscriber> subscriber_;
  };

  std::unique_ptr<Forwarder> forwarder_;
  const std::shared_ptr<Flowable> upstream_;
};

} // operators
} // yarpl

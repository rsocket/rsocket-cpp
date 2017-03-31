// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iostream>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/flowable/utils/SubscriptionHelper.h"

namespace yarpl {
namespace observable {
template <typename T>
class Observable;
}
}

namespace yarpl {
namespace flowable {
namespace sources {

template <typename T>
class FlowableFromObservableSubscription
    : public reactivestreams_yarpl::Subscription {
  friend class TheObserver;

 public:
  FlowableFromObservableSubscription(
      std::shared_ptr<yarpl::observable::Observable<T>> observable,
      std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> s)
      : observable_(std::move(observable)), subscriber_(std::move(s)) {}

  ~FlowableFromObservableSubscription() {
    // TODO remove this once happy with it
    std::cout << "DESTROY FlowableFromObservableSubscription!!!" << std::endl;
  }

  FlowableFromObservableSubscription(FlowableFromObservableSubscription&&) =
      delete;

  FlowableFromObservableSubscription(
      const FlowableFromObservableSubscription&) = delete;

  FlowableFromObservableSubscription& operator=(
      FlowableFromObservableSubscription&&) = delete;

  FlowableFromObservableSubscription& operator=(
      const FlowableFromObservableSubscription&) = delete;

  void start() {
    subscriber_->onSubscribe(this);
  }

  void request(int64_t n) override {
    if (n <= 0) {
      return;
    }
    int64_t r = internal::SubscriptionHelper::addCredits(&requested_, n);
    if (r <= 0) {
      return;
    }

    if (!started) {
      bool expected = false;
      if (started.compare_exchange_strong(expected, true)) {
        observable_->subscribe(std::make_unique<TheObserver>(this));
      }
    }
  }

  void cancel() override {
    if (yarpl::flowable::internal::SubscriptionHelper::addCancel(&requested_)) {
      // if this is the first time calling cancel, send the cancel
      // TODO hook up cancellation here
      // then try to delete if this thread wins the lock
      tryDelete();
    }
  }

 private:
  std::shared_ptr<yarpl::observable::Observable<T>> observable_;
  std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> subscriber_;
  std::atomic_bool started{false};
  std::atomic<int64_t> requested_{0};

  void tryDelete() {
    // only one thread can call delete

    // TODO when is it safe to delete?
    //    if (emitting_.fetch_add(1) == 0) {
    //      // TODO remove this cout once happy with it
    //      std::cout << "Delete FlowableSubscriptionSync" << std::endl;
    //      delete this;
    //    }
  }

  void onNext(const T& t) {
    if (requested_ > 0) {
      subscriber_->onNext(t);
      yarpl::flowable::internal::SubscriptionHelper::consumeCredits(
          &requested_, 1);
    }
    // drop anything else
  }

  void onNext(T&& t) {
    if (requested_ > 0) {
      subscriber_->onNext(std::move(t));
      yarpl::flowable::internal::SubscriptionHelper::consumeCredits(
          &requested_, 1);
    }
    // drop anything else
  }

  void onComplete() {
    subscriber_->onComplete();
  }

  void onError(const std::exception_ptr error) {
    subscriber_->onError(error);
  }

  /**
   * This will be used to subscribe to the upstream Observable.
   *
   * The FlowableFromObservableSubscription* contained by it will live
   * until we send terminal events or are cancelled.
   *
   * NOTE on lifecycle: This class can be deleted BEFORE
   * the Flowable which is represented by the subscription above.
   * This would happen if the Observable sends an onComplete/onError,
   * so we MUST not assume TheObserver will exist and retain
   * references to it.
   */
  class TheObserver : public yarpl::observable::Observer<T> {
   public:
    explicit TheObserver(FlowableFromObservableSubscription<T>* fs) : fs_(fs) {}

    void onSubscribe(yarpl::observable::Subscription* os) override {
      os_ = os;
    }

    void onNext(const T& t) override {
      fs_->onNext(t);
    }

    void onNext(T&& t) override {
      fs_->onNext(t);
    }

    void onComplete() override {
      fs_->onComplete();
    }

    void onError(const std::exception_ptr error) override {
      fs_->onError(error);
    }

    /**
     * Allow the Flowable Subscriber to cancel the Observable.
     */
    void cancel() {
      os_->cancel();
    }

   private:
    FlowableFromObservableSubscription<T>* fs_;
    yarpl::observable::Subscription* os_;
  };
};
}
}
}

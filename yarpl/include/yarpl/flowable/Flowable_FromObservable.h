// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Synchronized.h>
#include <deque>
#include "yarpl/Flowable.h"
#include "yarpl/utils/credits.h"

namespace yarpl {
namespace observable {
template <typename T>
class Observable;
}
}

class MissingBackpressureException;

namespace yarpl {
namespace flowable {

namespace details {

template <typename T>
class FlowableFromObservableSubscription : public flowable::Subscription,
                                           public observable::Observer<T> {
 public:
  FlowableFromObservableSubscription(
      std::shared_ptr<observable::Observable<T>> observable,
      std::shared_ptr<flowable::Subscriber<T>> subscriber)
      : observable_(std::move(observable)),
        subscriber_(std::move(subscriber)) {}

  FlowableFromObservableSubscription(FlowableFromObservableSubscription&&) =
      delete;

  FlowableFromObservableSubscription(
      const FlowableFromObservableSubscription&) = delete;
  FlowableFromObservableSubscription& operator=(
      FlowableFromObservableSubscription&&) = delete;
  FlowableFromObservableSubscription& operator=(
      const FlowableFromObservableSubscription&) = delete;

  void request(int64_t n) override {
    if (n <= 0) {
      return;
    }
    auto r = credits::add(&requested_, n);
    if (r <= 0) {
      return;
    }

    // it is possible that after calling subscribe or in onCreditsAvailable
    // methods, there will be a stream of
    // onNext calls which the processing chain might cancel. The cancel signal
    // will remove all references to this class and we need to keep this
    // instance around to finish this method
    auto thisPtr = this->ref_from_this(this);

    if (!started.exchange(true)) {
      observable_->subscribe(this->ref_from_this(this));

      // the credits might have changed since subscribe
      r = requested_.load();
    }

    if (r > 0) {
      onCreditsAvailable(r);
    }
  }

  void cancel() override {
    if (observable::Observer<T>::isUnsubscribedOrTerminated()) {
      return;
    }
    observable::Observer<T>::subscription()->cancel();
    credits::cancel(&requested_);
  }

  // Observer override
  void onNext(T t) override {
    if (requested_ > 0) {
      subscriber_->onNext(std::move(t));
      credits::consume(&requested_, 1);
      return;
    }
    onNextWithoutCredits(std::move(t));
  }

  // Observer override
  void onComplete() override {
    if (observable::Observer<T>::isUnsubscribedOrTerminated()) {
      return;
    }
    auto subscriber = std::move(subscriber_);
    subscriber->onComplete();
    observable::Observer<T>::onComplete();
  }

  // Observer override
  void onError(folly::exception_wrapper error) override {
    if (observable::Observer<T>::isUnsubscribedOrTerminated()) {
      return;
    }
    auto subscriber = std::move(subscriber_);
    subscriber->onError(std::move(error));
    observable::Observer<T>::onError(folly::exception_wrapper());
  }

 protected:
  virtual void onCreditsAvailable(int64_t /*credits*/) {}
  virtual void onNextWithoutCredits(T /*t*/) {
    // by default drop anything else received while we don't have credits
  }

  std::shared_ptr<observable::Observable<T>> observable_;
  std::shared_ptr<flowable::Subscriber<T>> subscriber_;
  std::atomic_bool started{false};
  std::atomic<int64_t> requested_{0};
};

template <typename T>
using FlowableFromObservableSubscriptionDropStrategy =
    FlowableFromObservableSubscription<T>;

template <typename T>
class FlowableFromObservableSubscriptionErrorStrategy
    : public FlowableFromObservableSubscription<T> {
  using Super = FlowableFromObservableSubscription<T>;

 public:
  using Super::FlowableFromObservableSubscription;

 private:
  void onNextWithoutCredits(T /*t*/) override {
    if (observable::Observer<T>::isUnsubscribedOrTerminated()) {
      return;
    }
    Super::onError(MissingBackpressureException());
    Super::cancel();
  }
};

template <typename T>
class FlowableFromObservableSubscriptionBufferStrategy
    : public FlowableFromObservableSubscription<T> {
  using Super = FlowableFromObservableSubscription<T>;

 public:
  using Super::FlowableFromObservableSubscription;

 private:
  void onComplete() override {
    if (!buffer_->empty()) {
      // we have buffered some items so we will defer delivering on complete for
      // later
      completed_ = true;
    } else {
      Super::onComplete();
    }
  }

  //
  // onError signal is delivered immediately by design
  //

  void onNextWithoutCredits(T t) override {
    if (Super::isUnsubscribed()) {
      return;
    }

    buffer_->push_back(std::move(t));
  }

  void onCreditsAvailable(int64_t credits) override {
    DCHECK(credits > 0);
    auto&& lockedBuffer = buffer_.wlock();
    while (credits-- > 0 && !lockedBuffer->empty()) {
      Super::onNext(std::move(lockedBuffer->front()));
      lockedBuffer->pop_front();
    }

    if (lockedBuffer->empty() && completed_) {
      Super::onComplete();
    }
  }

  folly::Synchronized<std::deque<T>> buffer_;
  std::atomic<bool> completed_{false};
};

template <typename T>
class FlowableFromObservableSubscriptionLatestStrategy
    : public FlowableFromObservableSubscription<T> {
  using Super = FlowableFromObservableSubscription<T>;

 public:
  using Super::FlowableFromObservableSubscription;

 private:
  void onComplete() override {
    if (storesLatest_) {
      // we have buffered an item so we will defer delivering on complete for
      // later
      completed_ = true;
    } else {
      Super::onComplete();
    }
  }

  //
  // onError signal is delivered immediately by design
  //

  void onNextWithoutCredits(T t) override {
    storesLatest_ = true;
    *latest_.wlock() = std::move(t);
  }

  void onCreditsAvailable(int64_t credits) override {
    DCHECK(credits > 0);
    if (storesLatest_) {
      storesLatest_ = false;
      Super::onNext(std::move(*latest_.wlock()));

      if (completed_) {
        Super::onComplete();
      }
    }
  }

  std::atomic<bool> storesLatest_{false};
  std::atomic<bool> completed_{false};
  folly::Synchronized<T> latest_;
};

template <typename T>
class FlowableFromObservableSubscriptionMissingStrategy
    : public FlowableFromObservableSubscription<T> {
  using Super = FlowableFromObservableSubscription<T>;

 public:
  using Super::FlowableFromObservableSubscription;

 private:
  void onNextWithoutCredits(T t) override {
    Super::onNext(std::move(t));
  }
};
}
}
}

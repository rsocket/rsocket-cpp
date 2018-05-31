// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Synchronized.h>
#include <deque>
#include "yarpl/Common.h"
#include "yarpl/Flowable.h"
#include "yarpl/utils/credits.h"

namespace yarpl {
namespace observable {
template <typename T>
class Observable;

template <typename T>
class Observer;
} // namespace observable

template <typename T>
class BackpressureStrategyBase : public IBackpressureStrategy<T>,
                                 public flowable::Subscription,
                                 public observable::Observer<T> {
 protected:
  //
  // the following methods are to be overridden
  //
  virtual void onCreditsAvailable(int64_t /*credits*/) = 0;
  virtual void onNextWithoutCredits(T /*t*/) = 0;

 public:
  void init(
      std::shared_ptr<observable::Observable<T>> observable,
      std::shared_ptr<flowable::Subscriber<T>> subscriber) override {
    observable_ = std::move(observable);
    subscriber_ = std::move(subscriber);
    subscriber_->onSubscribe(this->ref_from_this(this));
    observable_->subscribe(this->ref_from_this(this));
  }

  BackpressureStrategyBase() = default;
  BackpressureStrategyBase(BackpressureStrategyBase&&) = delete;

  BackpressureStrategyBase(const BackpressureStrategyBase&) = delete;
  BackpressureStrategyBase& operator=(BackpressureStrategyBase&&) = delete;
  BackpressureStrategyBase& operator=(const BackpressureStrategyBase&) = delete;

  // only for testing purposes
  void setTestSubscriber(std::shared_ptr<flowable::Subscriber<T>> subscriber) {
    subscriber_ = std::move(subscriber);
    subscriber_->onSubscribe(this->ref_from_this(this));
  }

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
    subscriber_ = nullptr;
    observable_.reset();
  }

  // Observer override
  void onNext(T t) override {
    if (observable::Observer<T>::isUnsubscribedOrTerminated()) {
      return;
    }
    if (requested_ > 0) {
      downstreamOnNext(std::move(t));
      return;
    }
    onNextWithoutCredits(std::move(t));
  }

  // Observer override
  void onComplete() override {
    downstreamOnComplete();
  }

  // Observer override
  void onError(folly::exception_wrapper ex) override {
    downstreamOnError(std::move(ex));
  }

  virtual void downstreamOnNext(T t) {
    credits::consume(&requested_, 1);
    subscriber_->onNext(std::move(t));
  }

  void downstreamOnComplete() {
    if (observable::Observer<T>::isUnsubscribedOrTerminated()) {
      return;
    }
    auto subscriber = std::move(subscriber_);
    subscriber->onComplete();
    observable::Observer<T>::onComplete();
    observable_.reset();
  }

  void downstreamOnError(folly::exception_wrapper error) {
    if (observable::Observer<T>::isUnsubscribedOrTerminated()) {
      return;
    }
    auto subscriber = std::move(subscriber_);
    subscriber->onError(std::move(error));
    observable::Observer<T>::onError(folly::exception_wrapper());
    observable_.reset();
  }

  void downstreamOnErrorAndCancel(folly::exception_wrapper error) {
    if (observable::Observer<T>::isUnsubscribedOrTerminated()) {
      return;
    }
    auto subscriber = std::move(subscriber_);
    subscriber->onError(std::move(error));

    observable_.reset();
    observable::Observer<T>::subscription()->cancel();
  }

 private:
  std::shared_ptr<observable::Observable<T>> observable_;
  std::shared_ptr<flowable::Subscriber<T>> subscriber_;
  std::atomic<int64_t> requested_{0};
};

template <typename T>
class DropBackpressureStrategy : public BackpressureStrategyBase<T> {
 public:
  void onCreditsAvailable(int64_t /*credits*/) override {}
  void onNextWithoutCredits(T /*t*/) override {
    // drop anything while we don't have credits
  }
};

template <typename T>
class ErrorBackpressureStrategy : public BackpressureStrategyBase<T> {
  using Super = BackpressureStrategyBase<T>;

  void onCreditsAvailable(int64_t /*credits*/) override {}

  void onNextWithoutCredits(T /*t*/) override {
    Super::downstreamOnErrorAndCancel(flowable::MissingBackpressureException());
  }
};

template <typename T>
class BufferBackpressureStrategy : public BackpressureStrategyBase<T> {
 public:
  static constexpr size_t kNoLimit = 0;

  explicit BufferBackpressureStrategy(size_t bufferSizeLimit = kNoLimit)
      : bufferSizeLimit_(bufferSizeLimit) {}

 private:
  using Super = BackpressureStrategyBase<T>;

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
    {
      auto buffer = buffer_.wlock();
      if (bufferSizeLimit_ == kNoLimit || buffer->size() < bufferSizeLimit_) {
        buffer->push_back(std::move(t));
        return;
      }
    }
    Super::downstreamOnErrorAndCancel(flowable::MissingBackpressureException());
  }

  void onCreditsAvailable(int64_t credits) override {
    DCHECK(credits > 0);
    auto&& lockedBuffer = buffer_.wlock();
    while (credits-- > 0 && !lockedBuffer->empty()) {
      Super::downstreamOnNext(std::move(lockedBuffer->front()));
      lockedBuffer->pop_front();
    }

    if (lockedBuffer->empty() && completed_) {
      Super::onComplete();
    }
  }

  folly::Synchronized<std::deque<T>> buffer_;
  std::atomic<bool> completed_{false};
  const size_t bufferSizeLimit_;
};

template <typename T>
class LatestBackpressureStrategy : public BackpressureStrategyBase<T> {
  using Super = BackpressureStrategyBase<T>;

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
      Super::downstreamOnNext(std::move(*latest_.wlock()));

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
class MissingBackpressureStrategy : public BackpressureStrategyBase<T> {
  using Super = BackpressureStrategyBase<T>;

  void onCreditsAvailable(int64_t /*credits*/) override {}

  void onNextWithoutCredits(T t) override {
    // call onNext anyways (and potentially violating the protocol)
    Super::downstreamOnNext(std::move(t));
  }
};

template <typename T>
std::shared_ptr<IBackpressureStrategy<T>> IBackpressureStrategy<T>::buffer() {
  return std::make_shared<BufferBackpressureStrategy<T>>();
}

template <typename T>
std::shared_ptr<IBackpressureStrategy<T>> IBackpressureStrategy<T>::drop() {
  return std::make_shared<DropBackpressureStrategy<T>>();
}

template <typename T>
std::shared_ptr<IBackpressureStrategy<T>> IBackpressureStrategy<T>::error() {
  return std::make_shared<ErrorBackpressureStrategy<T>>();
}

template <typename T>
std::shared_ptr<IBackpressureStrategy<T>> IBackpressureStrategy<T>::latest() {
  return std::make_shared<LatestBackpressureStrategy<T>>();
}

template <typename T>
std::shared_ptr<IBackpressureStrategy<T>> IBackpressureStrategy<T>::missing() {
  return std::make_shared<MissingBackpressureStrategy<T>>();
}

} // namespace yarpl

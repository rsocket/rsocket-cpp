// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <queue>

#include <folly/ExceptionWrapper.h>
#include <folly/Function.h>
#include <folly/Memory.h>
#include <folly/futures/QueuedImmediateExecutor.h>
#include <folly/io/IOBuf.h>

#include "src/ConnectionAutomaton.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

/// Instead of calling into the respective Base methods, schedules signals
/// delivery on an executor. Non-signal methods are simply forwarded.
///
/// Uses lazy method instantiantiation trick, see LoggingMixin.
template <typename Base>
class ExecutorMixin : public Base {
 public:
  using Base::Base;

  ~ExecutorMixin() {}

  /// The mixin starts in a queueing mode, where it merely queues signal
  /// deliveries until ::start is invoked.
  ///
  /// Calling into this method may deliver all enqueued signals immediately.
  void start() {
    if (pendingSignals_) {
      runInExecutor([pending_signals = std::move(pendingSignals_)]() mutable {
        for (auto& signal : *pending_signals) {
          signal();
        }
      });
    }
  }

  /// @{
  /// Publisher<Payload>
  void subscribe(Subscriber<Payload>& subscriber) {
    // This call punches through the executor-enforced ordering, to ensure that
    // the Subscriber pointer is set as soon as possible.
    // More esoteric reason: this is not a signal in ReactiveStreams language.
    Base::subscribe(subscriber);
  }
  /// @}

  /// @{
  /// Subscription
  void request(size_t n) {
    runInExecutor(std::bind(&Base::request, this, n));
  }

  void cancel() {
    runInExecutor(std::bind(&Base::cancel, this));
  }
  /// @}

  /// @{
  /// Subscriber<Payload>
  void onSubscribe(Subscription& subscription) {
    // This call punches through the executor-enforced ordering, to ensure that
    // the Subscription pointer is set as soon as possible.
    // More esoteric reason: this is not a signal in ReactiveStreams language.
    Base::onSubscribe(subscription);
  }

  void onNext(Payload payload) {
    runInExecutor([ this, payload = std::move(payload) ]() mutable {
      Base::onNext(std::move(payload));
    });
  }

  void onComplete() {
    runInExecutor(std::bind(&Base::onComplete, this));
  }

  void onError(folly::exception_wrapper ex) {
    runInExecutor([ this, ex = std::move(ex) ]() mutable {
      Base::onError(std::move(ex));
    });
  }
  /// @}

  /// @{
  void endStream(StreamCompletionSignal signal) {
    Base::endStream(signal);
  }
  /// @}

 protected:
  /// @{
  template <typename Frame>
  void onNextFrame(Frame&& frame) {
    Base::onNextFrame(std::move(frame));
  }

  void onBadFrame() {
    Base::onBadFrame();
  }
  /// @}

  std::ostream& logPrefix(std::ostream& os) {
    return os << "ExecutorMixin(" << &this->connection_ << ", "
              << this->streamId_ << "): ";
  }

 private:
  template <typename F>
  void runInExecutor(F&& func) {
    if (pendingSignals_) {
      pendingSignals_->emplace_back(std::forward<F>(func));
    } else {
      folly::QueuedImmediateExecutor::addStatic(std::forward<F>(func));
    }
  }

  using PendingSignals = std::vector<folly::Function<void()>>;
  std::unique_ptr<PendingSignals> pendingSignals_{
      folly::make_unique<PendingSignals>()};
};
}

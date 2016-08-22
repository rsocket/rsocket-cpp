// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <queue>

#include <folly/ExceptionWrapper.h>
#include <folly/Memory.h>
#include <folly/MoveWrapper.h>
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
      auto movedSignals = folly::makeMoveWrapper(std::move(pendingSignals_));
      runInExecutor([movedSignals]() mutable {
        for (auto& signal : **movedSignals) {
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
    auto movedPayload = folly::makeMoveWrapper(std::move(payload));
    runInExecutor(
        [this, movedPayload]() mutable { Base::onNext(movedPayload.move()); });
  }

  void onComplete() {
    runInExecutor(std::bind(&Base::onComplete, this));
  }

  void onError(folly::exception_wrapper ex) {
    auto movedEx = folly::makeMoveWrapper(std::move(ex));
    runInExecutor([this, movedEx]() mutable { Base::onError(movedEx.move()); });
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
      pendingSignals_->emplace_back(func);
    } else {
      folly::QueuedImmediateExecutor::addStatic(func);
    }
  }

  using PendingSignals = std::vector<std::function<void()>>;
  std::unique_ptr<PendingSignals> pendingSignals_{
      folly::make_unique<PendingSignals>()};
};
}

// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <queue>

#include <folly/ExceptionWrapper.h>
#include <folly/Executor.h>
#include <folly/Memory.h>
#include <folly/MoveWrapper.h>
#include <folly/io/IOBuf.h>

#include "src/ConnectionAutomaton.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

folly::Executor& defaultExecutor();

class ExecutorBase {
 public:
  explicit ExecutorBase(
      folly::Executor& executor = defaultExecutor(),
      bool startExecutor = true);

  /// We start in a queueing mode, where it merely queues signal
  /// deliveries until ::start is invoked.
  ///
  /// Calling into this method may deliver all enqueued signals immediately.
  void start();

 protected:
  template <typename F>
  void runInExecutor(F&& func) {
    if (pendingSignals_) {
      pendingSignals_->emplace_back(func);
    } else {
      executor_.add(std::move(func));
    }
  }

 private:
  using PendingSignals = std::vector<std::function<void()>>;
  std::unique_ptr<PendingSignals> pendingSignals_;
  folly::Executor& executor_;
};

/// Instead of calling into the respective Base methods, schedules signals
/// delivery on an executor. Non-signal methods are simply forwarded.
///
/// Uses lazy method instantiantiation trick, see LoggingMixin.
template <typename Base>
class ExecutorMixin : public Base,
                      public ExecutorBase,
                      public std::enable_shared_from_this<ExecutorMixin<Base>> {
  static_assert(
      !std::is_base_of<std::enable_shared_from_this<Base>, Base>::value,
      "enable_shared_from_this is already inherited");

 public:
  struct Parameters : Base::Parameters {
    Parameters(
        const typename Base::Parameters& baseParams,
        folly::Executor& _executor)
        : Base::Parameters(baseParams), executor(_executor) {}
    folly::Executor& executor;
  };

  ExecutorMixin(const Parameters& params)
      : Base(params), ExecutorBase(params.executor, false) {}

  /// @{
  /// Publisher<Payload>
  void subscribe(std::shared_ptr<Subscriber<Payload>> subscriber) {
    // This call punches through the executor-enforced ordering, to ensure that
    // the Subscriber pointer is set as soon as possible.
    // More esoteric reason: this is not a signal in ReactiveStreams language.
    Base::subscribe(std::move(subscriber));
  }
  /// @}

  /// @{
  /// Subscription
  void request(size_t n) {
    runInExecutor(std::bind(&Base::request, this->shared_from_this(), n));
  }

  void cancel() {
    runInExecutor(std::bind(&Base::cancel, this->shared_from_this()));
  }
  /// @}

  /// @{
  /// Subscriber<Payload>
  void onSubscribe(std::shared_ptr<Subscription> subscription) {
    // This call punches through the executor-enforced ordering, to ensure that
    // the Subscription pointer is set as soon as possible.
    // More esoteric reason: this is not a signal in ReactiveStreams language.
    Base::onSubscribe(std::move(subscription));
  }

  void onNext(Payload payload) {
    auto movedPayload = folly::makeMoveWrapper(std::move(payload));
    std::shared_ptr<Base> basePtr = this->shared_from_this();
    runInExecutor([basePtr, movedPayload]() mutable {
      basePtr->onNext(movedPayload.move());
    });
  }

  void onComplete() {
    runInExecutor(std::bind(&Base::onComplete, this->shared_from_this()));
  }

  void onError(folly::exception_wrapper ex) {
    auto movedEx = folly::makeMoveWrapper(std::move(ex));
    std::shared_ptr<Base> basePtr = this->shared_from_this();
    runInExecutor(
        [basePtr, movedEx]() mutable { basePtr->onError(movedEx.move()); });
  }
  /// @}

  /// @{
  void endStream(StreamCompletionSignal signal) {
    Base::endStream(signal);
  }
  /// @}

  std::ostream& logPrefix(std::ostream& os) {
    return os << "ExecutorMixin(" << &this->connection_ << ", "
              << this->streamId_ << "): ";
  }

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
};
} // reactivesocket

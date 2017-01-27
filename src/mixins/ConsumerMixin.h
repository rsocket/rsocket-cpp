// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <cstddef>
#include <iostream>
#include "src/AllowanceSemaphore.h"
#include "src/Common.h"
#include "src/NullRequestHandler.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/SmartPointers.h"
#include "src/automata/StreamAutomatonBase.h"

namespace reactivesocket {

enum class StreamCompletionSignal;

/// A mixin that represents a flow-control-aware consumer of data.
template <typename Frame>
class ConsumerMixin : public StreamAutomatonBase, public SubscriptionBase {
  using Base = StreamAutomatonBase;

 public:
  struct Parameters : Base::Parameters {
    Parameters(
        const typename Base::Parameters& baseParams,
        folly::Executor& _executor)
        : Base::Parameters(baseParams), executor(_executor) {}
    folly::Executor& executor;
  };

  explicit ConsumerMixin(const Parameters& params)
      : ExecutorBase(params.executor), Base(params) {}

  /// Adds implicit allowance.
  ///
  /// This portion of allowance will not be synced to the remote end, but will
  /// count towards the limit of allowance the remote PublisherMixin may use.
  void addImplicitAllowance(size_t n) {
    allowance_.release(n);
  }

  /// @{
  void subscribe(std::shared_ptr<Subscriber<Payload>> subscriber) {
    if (Base::isTerminated()) {
      subscriber->onSubscribe(std::make_shared<NullSubscription>());
      subscriber->onComplete();
      return;
    }

    DCHECK(!consumingSubscriber_);
    consumingSubscriber_.reset(std::move(subscriber));
    consumingSubscriber_.onSubscribe(shared_from_this());
  }

  void generateRequest(size_t n) {
    allowance_.release(n);
    pendingAllowance_.release(n);
    sendRequests();
  }
  /// @}

  std::ostream& logPrefix(std::ostream& os) {
    return os << "ConsumerMixin(" << &this->connection_ << ", "
              << this->streamId_ << "): ";
  }

 protected:
  /// @{
  void endStream(StreamCompletionSignal signal) override {
    if (signal == StreamCompletionSignal::GRACEFUL) {
      consumingSubscriber_.onComplete();
    } else {
      consumingSubscriber_.onError(
          StreamInterruptedException(static_cast<int>(signal)));
    }
    Base::endStream(signal);
  }

  void pauseStream(RequestHandler& requestHandler) override {
    if (consumingSubscriber_) {
      requestHandler.onSubscriberPaused(consumingSubscriber_);
    }
  }

  void resumeStream(RequestHandler& requestHandler) override {
    if (consumingSubscriber_) {
      requestHandler.onSubscriberResumed(consumingSubscriber_);
    }
  }

  void processPayload(Frame&&);

  void onError(folly::exception_wrapper ex);
  /// @}

 private:
  // we don't want derived classes to call these methods.
  // derived classes should be calling implementation methods, not the top level
  // methods which are for the application code.
  // avoiding potential bugs..
  using SubscriptionBase::request;
  using SubscriptionBase::cancel;

  void sendRequests();

  void handleFlowControlError();

  /// A Subscriber that will consume payloads.
  /// This mixin is responsible for delivering a terminal signal to the
  /// Subscriber once the stream ends.
  reactivestreams::SubscriberPtr<Subscriber<Payload>> consumingSubscriber_;

  /// A total, net allowance (requested less delivered) by this consumer.
  AllowanceSemaphore allowance_;
  /// An allowance that have yet to be synced to the other end by sending
  /// REQUEST_N frames.
  AllowanceSemaphore pendingAllowance_;
};
}

#include "src/mixins/ConsumerMixin-inl.h"

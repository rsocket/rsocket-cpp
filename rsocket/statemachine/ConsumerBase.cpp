// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/statemachine/ConsumerBase.h"

#include <algorithm>

#include <glog/logging.h>

namespace rsocket {

void ConsumerBase::subscribe(
    std::shared_ptr<yarpl::flowable::Subscriber<Payload>> subscriber) {
  if (state_ == State::CLOSED) {
    subscriber->onSubscribe(yarpl::flowable::Subscription::create());
    subscriber->onComplete();
    return;
  }

  DCHECK(!consumingSubscriber_);
  consumingSubscriber_ = std::move(subscriber);
  consumingSubscriber_->onSubscribe(shared_from_this());
}

void ConsumerBase::cancelConsumer() {
  state_ = State::CLOSED;
  VLOG(5) << "ConsumerBase::cancelConsumer()";
  consumingSubscriber_ = nullptr;
}

void ConsumerBase::addImplicitAllowance(size_t n) {
  allowance_.add(n);
  activeRequests_.add(n);
}

void ConsumerBase::generateRequest(size_t n) {
  allowance_.add(n);
  pendingAllowance_.add(n);
  sendRequests();
}

void ConsumerBase::endStream(StreamCompletionSignal signal) {
  VLOG(5) << "ConsumerBase::endStream(" << signal << ")";
  state_ = State::CLOSED;
  if (auto subscriber = std::move(consumingSubscriber_)) {
    if (signal == StreamCompletionSignal::COMPLETE ||
        signal == StreamCompletionSignal::CANCEL) { // TODO: remove CANCEL
      VLOG(5) << "Closing ConsumerBase subscriber with calling onComplete";
      subscriber->onComplete();
    } else {
      VLOG(5) << "Closing ConsumerBase subscriber with calling onError";
      subscriber->onError(StreamInterruptedException(static_cast<int>(signal)));
    }
  }
}

size_t ConsumerBase::getConsumerAllowance() const {
  return allowance_.get();
}

void ConsumerBase::processPayload(Payload&& payload, bool onNext) {
  if (!payload && !onNext) {
    return;
  }

  // Frames carrying application-level payloads are taken into account when
  // figuring out flow control allowance.
  if (!allowance_.tryConsume(1) || !activeRequests_.tryConsume(1)) {
    handleFlowControlError();
    return;
  }

  sendRequests();
  if (consumingSubscriber_) {
    consumingSubscriber_->onNext(std::move(payload));
  } else {
    LOG(ERROR) << "Consuming subscriber is missing, might be a race on "
               << "cancel/onNext";
  }
}

void ConsumerBase::completeConsumer() {
  state_ = State::CLOSED;
  VLOG(5) << "ConsumerBase::completeConsumer()";
  if (auto subscriber = std::move(consumingSubscriber_)) {
    subscriber->onComplete();
  }
}

void ConsumerBase::errorConsumer(folly::exception_wrapper ex) {
  state_ = State::CLOSED;
  VLOG(5) << "ConsumerBase::errorConsumer()";
  if (auto subscriber = std::move(consumingSubscriber_)) {
    subscriber->onError(std::move(ex));
  }
}

void ConsumerBase::sendRequests() {
  auto toSync =
      std::min<size_t>(pendingAllowance_.get(), Frame_REQUEST_N::kMaxRequestN);
  auto actives = activeRequests_.get();
  if (actives < (toSync + 1) / 2) {
    toSync = toSync - actives;
    toSync = pendingAllowance_.consumeUpTo(toSync);
    if (toSync > 0) {
      writeRequestN(static_cast<uint32_t>(toSync));
      activeRequests_.add(toSync);
    }
  }
}

void ConsumerBase::handleFlowControlError() {
  if (auto subscriber = std::move(consumingSubscriber_)) {
    subscriber->onError(std::runtime_error("Surplus response"));
  }
  writeInvalidError("Flow control error");
  endStream(StreamCompletionSignal::ERROR);
  removeFromWriter();
}

} // namespace rsocket

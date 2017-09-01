// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/statemachine/ConsumerBase.h"

#include <algorithm>

#include <glog/logging.h>

#include "rsocket/Payload.h"
#include "yarpl/flowable/Subscription.h"

namespace rsocket {

using namespace yarpl;
using namespace yarpl::flowable;

void ConsumerBase::subscribe(
    Reference<yarpl::flowable::Subscriber<Payload>> subscriber) {
  if (isTerminated()) {
    subscriber->onSubscribe(yarpl::flowable::Subscription::empty());
    subscriber->onComplete();
    return;
  }

  DCHECK(!consumingSubscriber_);
  consumingSubscriber_ = std::move(subscriber);
  consumingSubscriber_->onSubscribe(get_ref(this));
}

void ConsumerBase::checkConsumerRequest() {
  // we are either responding and subscribe method was called
  // or we are already terminated
  CHECK((state_ == State::RESPONDING) == !!consumingSubscriber_);
}

// TODO: this is probably buggy and misused and not needed (when
// completeConsumer exists)
void ConsumerBase::cancelConsumer() {
  state_ = State::CLOSED;
  consumingSubscriber_ = nullptr;
}

void ConsumerBase::generateRequest(size_t n) {
  allowance_.release(n);
  pendingAllowance_.release(n);
  sendRequests();
}

void ConsumerBase::endStream(StreamCompletionSignal signal) {
  VLOG(5) << "ConsumerBase::endStream(" << signal << ")";
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
  StreamStateMachineBase::endStream(signal);
}

size_t ConsumerBase::getConsumerAllowance() const {
  return allowance_.getValue();
}

void ConsumerBase::processPayload(Payload&& payload, bool onNext) {
  if (payload || onNext) {
    // Frames carry application-level payloads are taken into account when
    // figuring out flow control allowance.
    if (allowance_.tryAcquire()) {
      sendRequests();
      consumingSubscriber_->onNext(std::move(payload));
    } else {
      handleFlowControlError();
      return;
    }
  }
}

void ConsumerBase::completeConsumer() {
  state_ = State::CLOSED;
  if (auto subscriber = std::move(consumingSubscriber_)) {
    subscriber->onComplete();
  }
}

void ConsumerBase::errorConsumer(folly::exception_wrapper ex) {
  state_ = State::CLOSED;
  if (auto subscriber = std::move(consumingSubscriber_)) {
    subscriber->onError(std::move(ex));
  }
}

void ConsumerBase::sendRequests() {
  // TODO(stupaq): batch if remote end has some spare allowance
  // TODO(stupaq): limit how much is synced to the other end
  size_t toSync = Frame_REQUEST_N::kMaxRequestN;
  toSync = pendingAllowance_.drainWithLimit(toSync);
  if (toSync > 0) {
    writeRequestN(static_cast<uint32_t>(toSync));
  }
}

void ConsumerBase::handleFlowControlError() {
  if (auto subscriber = std::move(consumingSubscriber_)) {
    subscriber->onError(std::runtime_error("surplus response"));
  }
  errorStream("flow control error");
}
}

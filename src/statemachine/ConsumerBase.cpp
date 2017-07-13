// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/statemachine/ConsumerBase.h"

#include <glog/logging.h>
#include <algorithm>

#include "src/Payload.h"
#include "yarpl/flowable/Subscription.h"

namespace rsocket {

using namespace yarpl;
using namespace yarpl::flowable;

void ConsumerBase::subscribe(
    Reference<yarpl::flowable::Subscriber<Payload>> subscriber) {
  if (Base::isTerminated()) {
    subscriber->onSubscribe(yarpl::flowable::Subscription::empty());
    subscriber->onComplete();
    return;
  }

  DCHECK(!consumingSubscriber_);
  consumingSubscriber_ = std::move(subscriber);
  consumingSubscriber_->onSubscribe(Reference<Subscription>(this));
}

void ConsumerBase::checkConsumerRequest() {
  DCHECK(consumingSubscriber_);
  CHECK(state_ == State::RESPONDING);
}

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
  if (auto subscriber = std::move(consumingSubscriber_)) {
    if (signal == StreamCompletionSignal::COMPLETE ||
        signal == StreamCompletionSignal::CANCEL) { // TODO: remove CANCEL
      subscriber->onComplete();
    } else {
      subscriber->onError(std::make_exception_ptr(
          StreamInterruptedException(static_cast<int>(signal))));
    }
  }
  Base::endStream(signal);
}

//void ConsumerBase::pauseStream(RequestHandler& requestHandler) {
//  if (consumingSubscriber_) {
//    requestHandler.onSubscriberPaused(consumingSubscriber_);
//  }
//}
//
//void ConsumerBase::resumeStream(RequestHandler& requestHandler) {
//  if (consumingSubscriber_) {
//    requestHandler.onSubscriberResumed(consumingSubscriber_);
//  }
//}

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
    subscriber->onError(ex.to_exception_ptr());
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
    subscriber->onError(
        std::make_exception_ptr(std::runtime_error("surplus response")));
  }
  errorStream("flow control error");
}

}

// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/ConsumerBase.h"

#include <glog/logging.h>
#include <algorithm>
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

void ConsumerBase::subscribe(std::shared_ptr<Subscriber<Payload>> subscriber) {
  if (Base::isTerminated()) {
    subscriber->onSubscribe(std::make_shared<NullSubscription>());
    subscriber->onComplete();
    return;
  }

  DCHECK(!consumingSubscriber_);
  consumingSubscriber_ = std::move(subscriber);
  consumingSubscriber_->onSubscribe(shared_from_this());
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
      subscriber->onError(StreamInterruptedException(static_cast<int>(signal)));
    }
  }
  Base::endStream(signal);
}

void ConsumerBase::pauseStream(RequestHandler& requestHandler) {
  if (consumingSubscriber_) {
    requestHandler.onSubscriberPaused(consumingSubscriber_);
  }
}

void ConsumerBase::resumeStream(RequestHandler& requestHandler) {
  if (consumingSubscriber_) {
    requestHandler.onSubscriberResumed(consumingSubscriber_);
  }
}

void ConsumerBase::processPayload(Payload&& payload) {
  if (payload) {
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

void ConsumerBase::onError(folly::exception_wrapper ex) {
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

// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/RequestResponseRequester.h"
#include <folly/ExceptionWrapper.h>
#include <folly/MoveWrapper.h>
#include "src/Common.h"
#include "src/ConnectionAutomaton.h"
#include "src/RequestHandler.h"

namespace reactivesocket {

void RequestResponseRequester::subscribe(
    std::shared_ptr<Subscriber<Payload>> subscriber) {
  DCHECK(!isTerminated());
  DCHECK(!consumingSubscriber_);
  consumingSubscriber_ = std::move(subscriber);
  consumingSubscriber_->onSubscribe(SubscriptionBase::shared_from_this());
}

void RequestResponseRequester::requestImpl(size_t n) noexcept {
  if (n == 0) {
    return;
  }

  if (state_ == State::NEW) {
    state_ = State::REQUESTED;
    newStream(StreamType::REQUEST_RESPONSE, 1, std::move(initialPayload_));
  }

  if (payload_) {
    consumingSubscriber_->onNext(std::move(payload_));
    DCHECK(!payload_);
    closeStream(StreamCompletionSignal::COMPLETE);
  } else {
    waitingForPayload_ = true;
  }
}

void RequestResponseRequester::cancelImpl() noexcept {
  switch (state_) {
    case State::NEW:
      state_ = State::CLOSED;
      closeStream(StreamCompletionSignal::CANCEL);
      break;
    case State::REQUESTED: {
      state_ = State::CLOSED;
      cancelStream();
    } break;
    case State::CLOSED:
      break;
  }
}

void RequestResponseRequester::endStream(StreamCompletionSignal signal) {
  // to make sure we don't try to deliver the payload even if we had it
  // because requestImpl can be called even after endStream
  payload_.clear();

  switch (state_) {
    case State::NEW:
    case State::REQUESTED:
      // Spontaneous ::endStream signal means an error.
      DCHECK(StreamCompletionSignal::COMPLETE != signal);
      DCHECK(StreamCompletionSignal::CANCEL != signal);
      state_ = State::CLOSED;
      break;
    case State::CLOSED:
      break;
  }
  if (auto subscriber = std::move(consumingSubscriber_)) {
    if (signal == StreamCompletionSignal::COMPLETE ||
        signal == StreamCompletionSignal::CANCEL) { // TODO: remove CANCEL
      subscriber->onComplete();
    } else {
      subscriber->onError(StreamInterruptedException(static_cast<int>(signal)));
    }
  }
}

void RequestResponseRequester::onNextFrame(Frame_ERROR&& frame) {
  switch (state_) {
    case State::NEW:
      // Cannot receive a frame before sending the initial request.
      CHECK(false);
      break;
    case State::REQUESTED:
      state_ = State::CLOSED;
      if (auto subscriber = std::move(consumingSubscriber_)) {
        subscriber->onError(
            std::runtime_error(frame.payload_.moveDataToString()));
      }
      closeStream(StreamCompletionSignal::ERROR);
      break;
    case State::CLOSED:
      break;
  }
}

void RequestResponseRequester::onNextFrame(Frame_PAYLOAD&& frame) {
  switch (state_) {
    case State::NEW:
      // Cannot receive a frame before sending the initial request.
      CHECK(false);
      break;
    case State::REQUESTED:
      state_ = State::CLOSED;
      break;
    case State::CLOSED:
      // should not be receiving frames when closed
      // if we ended up here, we broke some internal invariant of the class
      CHECK(false);
      break;
  }

  if (!frame.payload_) {
    errorStream("payload expected");
    // will call endStream on the stream
    return;
  }

  if (waitingForPayload_) {
    consumingSubscriber_->onNext(std::move(frame.payload_));
    closeStream(StreamCompletionSignal::COMPLETE);
  } else {
    payload_ = std::move(frame.payload_);
    // we will just remember the payload and return it when request(n) is called
    // the stream will terminate right after
  }
}

void RequestResponseRequester::pauseStream(RequestHandler& requestHandler) {
  if (consumingSubscriber_) {
    requestHandler.onSubscriberPaused(consumingSubscriber_);
  }
}

void RequestResponseRequester::resumeStream(RequestHandler& requestHandler) {
  if (consumingSubscriber_) {
    requestHandler.onSubscriberResumed(consumingSubscriber_);
  }
}
}

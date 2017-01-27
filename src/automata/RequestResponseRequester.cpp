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
  consumingSubscriber_.reset(std::move(subscriber));
  consumingSubscriber_.onSubscribe(SubscriptionBase::shared_from_this());
}

void RequestResponseRequester::requestImpl(size_t n) noexcept {
  if (n == 0) {
    return;
  }

  if (state_ == State::NEW) {
    state_ = State::REQUESTED;
    Frame_REQUEST_RESPONSE frame(
        streamId_, FrameFlags_EMPTY, std::move(std::move(initialPayload_)));
    connection_->outputFrameOrEnqueue(frame.serializeOut());
  }

  if (payload_) {
    consumingSubscriber_.onNext(std::move(payload_));
    DCHECK(!payload_);
    connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
  } else {
    waitingForPayload_ = true;
  }
}

void RequestResponseRequester::cancelImpl() noexcept {
  switch (state_) {
    case State::NEW:
      state_ = State::CLOSED;
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
      break;
    case State::REQUESTED: {
      state_ = State::CLOSED;
      connection_->outputFrameOrEnqueue(Frame_CANCEL(streamId_).serializeOut());
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
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
      DCHECK(StreamCompletionSignal::GRACEFUL != signal);
      state_ = State::CLOSED;
      break;
    case State::CLOSED:
      break;
  }
  if (signal == StreamCompletionSignal::GRACEFUL) {
    consumingSubscriber_.onComplete();
  } else {
    consumingSubscriber_.onError(
        StreamInterruptedException(static_cast<int>(signal)));
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
      consumingSubscriber_.onError(
          std::runtime_error(frame.payload_.moveDataToString()));
      connection_->endStream(streamId_, StreamCompletionSignal::ERROR);
      break;
    case State::CLOSED:
      break;
  }
}

void RequestResponseRequester::onNextFrame(Frame_RESPONSE&& frame) {
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
    connection_->closeWithError(
        Frame_ERROR::invalid(streamId_, "payload expected"));
    // will call endStream on all streams, including this one
    return;
  }

  if (waitingForPayload_) {
    consumingSubscriber_.onNext(std::move(frame.payload_));
    connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
  } else {
    payload_ = std::move(frame.payload_);
    // we will just remember the payload and return it when request(n) is called
    // the stream will terminate right after
  }
}

std::ostream& RequestResponseRequester::logPrefix(std::ostream& os) {
  return os << " RequestResponseRequester(" << &connection_ << ", " << streamId_
            << "): ";
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

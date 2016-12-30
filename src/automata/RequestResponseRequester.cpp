// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/RequestResponseRequester.h"
#include <folly/ExceptionWrapper.h>
#include <folly/MoveWrapper.h>
#include "src/Common.h"
#include "src/ConnectionAutomaton.h"

namespace reactivesocket {

bool RequestResponseRequester::subscribe(
    std::shared_ptr<Subscriber<Payload>> subscriber) {
  DCHECK(!isTerminated());
  DCHECK(!consumingSubscriber_);
  consumingSubscriber_.reset(std::move(subscriber));
  // FIXME(lehecka): now it is possible to move it here
  // Subscriber::onSubscribe is delivered externally, as it may attempt to
  // synchronously deliver Subscriber::request.
  return true;
}

void RequestResponseRequester::onNext(Payload request) {
  auto movedPayload = folly::makeMoveWrapper(std::move(request));
  auto thisPtr =
      EnableSharedFromThisBase<RequestResponseRequester>::shared_from_this();
  runInExecutor([thisPtr, movedPayload]() mutable {
    thisPtr->onNextImpl(movedPayload.move());
  });
}

void RequestResponseRequester::onNextImpl(Payload request) {
  switch (state_) {
    case State::NEW: {
      state_ = State::REQUESTED;
      Frame_REQUEST_RESPONSE frame(
          streamId_, FrameFlags_EMPTY, std::move(std::move(request)));
      connection_->outputFrameOrEnqueue(frame.serializeOut());
      break;
    }
    case State::REQUESTED:
      // Cannot receive a request payload twice.
      CHECK(false);
      break;
    case State::CLOSED:
      break;
  }
}

void RequestResponseRequester::requestImpl(size_t n) {
  if (n == 0) {
    return;
  }

  if (payload_) {
    consumingSubscriber_.onNext(std::move(payload_));
    DCHECK(!payload_);
    // TODO: only one response is expected so we should close the stream
    //       after receiving it. Add unit tests for this.
  } else {
    waitingForPayload_ = true;
  }
}

void RequestResponseRequester::cancelImpl() {
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
    consumingSubscriber_.onError(StreamInterruptedException((int)signal));
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
  bool end = false;
  switch (state_) {
    case State::NEW:
      // Cannot receive a frame before sending the initial request.
      CHECK(false);
      break;
    case State::REQUESTED:
      // TODO: only one response is expected so we should close the stream
      //       after receiving it. Add unit tests for this.
      if (frame.header_.flags_ & FrameFlags_COMPLETE) {
        state_ = State::CLOSED;
        end = true;
      }
      break;
    case State::CLOSED:
      break;
  }
  if (waitingForPayload_ && frame.payload_) {
    consumingSubscriber_.onNext(std::move(frame.payload_));
  } else {
    payload_ = std::move(frame.payload_);
  }

  if (end) {
    connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
  }
}

std::ostream& RequestResponseRequester::logPrefix(std::ostream& os) {
  return os << " RequestResponseRequester(" << &connection_ << ", " << streamId_
            << "): ";
}
}

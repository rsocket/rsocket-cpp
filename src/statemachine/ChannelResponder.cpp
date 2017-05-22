// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/statemachine/ChannelResponder.h"
#include <folly/ExceptionString.h>

namespace rsocket {

using namespace yarpl;
using namespace yarpl::flowable;

void ChannelResponder::onSubscribe(
    Reference<Subscription> subscription) noexcept {
  if (ConsumerBase::isTerminated()) {
    subscription->cancel();
    return;
  }
  publisherSubscribe(std::move(subscription));
}

void ChannelResponder::onNext(Payload response) noexcept {
  debugCheckOnNextOnError();
  switch (state_) {
    case State::RESPONDING: {
      writePayload(std::move(response), false);
      break;
    }
    case State::CLOSED:
      break;
  }
}

void ChannelResponder::onComplete() noexcept {
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      releasePublisher();
      completeStream();
    } break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponder::onError(const std::exception_ptr ex) noexcept {
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      releasePublisher();
      applicationError(folly::exceptionStr(ex).toStdString());
    } break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponder::request(int64_t n) noexcept {
  switch (state_) {
    case State::RESPONDING:
      ConsumerBase::generateRequest(n);
      break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponder::cancel() noexcept {
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      releaseConsumer();
      completeStream();
    } break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponder::endStream(StreamCompletionSignal signal) {
  switch (state_) {
    case State::RESPONDING:
      // Spontaneous ::endStream signal means an error.
      DCHECK(StreamCompletionSignal::COMPLETE != signal);
      DCHECK(StreamCompletionSignal::CANCEL != signal);
      state_ = State::CLOSED;
      break;
    case State::CLOSED:
      break;
  }
  terminatePublisher(signal);
  ConsumerBase::endStream(signal);
}

// TODO: remove this unused function
void ChannelResponder::processInitialFrame(Frame_REQUEST_CHANNEL&& frame) {
  onNextPayloadFrame(
      frame.requestN_,
      std::move(frame.payload_),
      frame.header_.flagsComplete(),
      true);
}

void ChannelResponder::handlePayload(
    Payload&& payload,
    bool complete,
    bool flagsNext) {
  onNextPayloadFrame(0, std::move(payload), complete, flagsNext);
}

void ChannelResponder::onNextPayloadFrame(
    uint32_t requestN,
    Payload&& payload,
    bool complete,
    bool next) {
  bool end = false;
  switch (state_) {
    case State::RESPONDING:
      if (complete) {
        state_ = State::CLOSED;
        end = true;
      }
      break;
    case State::CLOSED:
      break;
  }

  processRequestN(requestN);
  processPayload(std::move(payload), next);

  if (end) {
    closeStream(StreamCompletionSignal::COMPLETE);
  }
}

void ChannelResponder::handleCancel() {
  switch (state_) {
    case State::RESPONDING:
      state_ = State::CLOSED;
      closeStream(StreamCompletionSignal::CANCEL);
      break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponder::handleRequestN(uint32_t n) {
  PublisherBase::processRequestN(n);
}
}

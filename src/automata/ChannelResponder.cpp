// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/ChannelResponder.h"

namespace reactivesocket {

void ChannelResponder::onSubscribeImpl(
    std::shared_ptr<Subscription> subscription) noexcept {
  if (ConsumerBase::isTerminated()) {
    subscription->cancel();
    return;
  }
  publisherSubscribe(subscription);
}

void ChannelResponder::onNextImpl(Payload response) noexcept {
  switch (state_) {
    case State::RESPONDING: {
      debugCheckOnNextOnCompleteOnError();
      writePayload(std::move(response), false);
      break;
    }
    case State::CLOSED:
      break;
  }
}

void ChannelResponder::onCompleteImpl() noexcept {
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      completeStream();
    } break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponder::onErrorImpl(folly::exception_wrapper ex) noexcept {
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      applicationError(ex.what().toStdString());
    } break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponder::requestImpl(size_t n) noexcept {
  switch (state_) {
    case State::RESPONDING:
      ConsumerBase::generateRequest(n);
      break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponder::cancelImpl() noexcept {
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
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

void ChannelResponder::processInitialFrame(Frame_REQUEST_CHANNEL&& frame) {
  onNextPayloadFrame(
      frame.requestN_,
      std::move(frame.payload_),
      frame.header_.flagsComplete(),
      true);
}

void ChannelResponder::onNextFrame(Frame_REQUEST_CHANNEL&& frame) {
  // TODO(t16487710): remove handling this frame when we remove support for
  // protocol version < 1.0
  processInitialFrame(std::move(frame));
}

void ChannelResponder::onNextFrame(Frame_PAYLOAD&& frame) {
  onNextPayloadFrame(
      0,
      std::move(frame.payload_),
      frame.header_.flagsComplete(),
      frame.header_.flagsNext());
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

void ChannelResponder::onNextFrame(Frame_CANCEL&& frame) {
  switch (state_) {
    case State::RESPONDING:
      state_ = State::CLOSED;
      closeStream(StreamCompletionSignal::CANCEL);
      break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponder::onNextFrame(Frame_REQUEST_N&& frame) {
  PublisherBase::processRequestN(frame.requestN_);
}
}

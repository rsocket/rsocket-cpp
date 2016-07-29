// Copyright 2004-present Facebook. All Rights Reserved.

#include "ChannelResponder.h"

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>

#include "src/ConnectionAutomaton.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

void ChannelResponderBase::onNext(Payload response) {
  switch (state_) {
    case State::RESPONDING:
      Base::onNext(std::move(response));
      break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponderBase::onComplete() {
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      Frame_RESPONSE frame(
          streamId_, FrameFlags_COMPLETE, FrameMetadata::empty(), nullptr);
      connection_->onNextFrame(frame);
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
    } break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponderBase::onError(folly::exception_wrapper ex) {
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      Frame_ERROR frame(
          streamId_,
          FrameFlags_EMPTY,
          ErrorCode::APPLICATION_ERROR,
          FrameMetadata::empty(),
          folly::IOBuf::copyBuffer(ex.what().toStdString()));
      connection_->onNextFrame(frame);
      connection_->endStream(streamId_, StreamCompletionSignal::ERROR);
    } break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponderBase::request(size_t n) {
  switch (state_) {
    case State::RESPONDING:
      Base::request(n);
    case State::CLOSED:
      break;
  }
}

void ChannelResponderBase::cancel() {
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      Frame_RESPONSE frame(
          streamId_, FrameFlags_COMPLETE, FrameMetadata::empty(), nullptr);
      connection_->onNextFrame(frame);
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
    } break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponderBase::endStream(StreamCompletionSignal signal) {
  switch (state_) {
    case State::RESPONDING:
      // Spontaneous ::endStream signal means an error.
      DCHECK(StreamCompletionSignal::GRACEFUL != signal);
      state_ = State::CLOSED;
      break;
    case State::CLOSED:
      break;
  }
  Base::endStream(signal);
}

void ChannelResponderBase::onNextFrame(Frame_REQUEST_CHANNEL& frame) {
  bool end = false;
  switch (state_) {
    case State::RESPONDING:
      if (frame.header_.flags_ & FrameFlags_COMPLETE) {
        state_ = State::CLOSED;
        end = true;
      }
      break;
    case State::CLOSED:
      break;
  }
  Base::onNextFrame(frame);
  if (end) {
    connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
  }
}

void ChannelResponderBase::onNextFrame(Frame_CANCEL& frame) {
  switch (state_) {
    case State::RESPONDING:
      state_ = State::CLOSED;
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
      break;
    case State::CLOSED:
      break;
  }
}

std::ostream& ChannelResponderBase::logPrefix(std::ostream& os) {
  return os << "ChannelResponder(" << &connection_ << ", " << streamId_
            << "): ";
}
}

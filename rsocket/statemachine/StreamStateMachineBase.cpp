// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/statemachine/StreamStateMachineBase.h"

#include <folly/io/IOBuf.h>

#include "rsocket/statemachine/RSocketStateMachine.h"
#include "rsocket/statemachine/StreamsWriter.h"

namespace rsocket {

void StreamStateMachineBase::handlePayload(Payload&&, bool, bool) {
  VLOG(4) << "Unexpected handlePayload";
}

void StreamStateMachineBase::handleRequestN(uint32_t) {
  VLOG(4) << "Unexpected handleRequestN";
}

void StreamStateMachineBase::handleError(folly::exception_wrapper) {
  closeStream(StreamCompletionSignal::ERROR);
}

void StreamStateMachineBase::handleCancel() {
  VLOG(4) << "Unexpected handleCancel";
}

size_t StreamStateMachineBase::getConsumerAllowance() const {
  return 0;
}

void StreamStateMachineBase::endStream(StreamCompletionSignal) {
  isTerminated_ = true;
}

void StreamStateMachineBase::newStream(
    StreamType streamType,
    uint32_t initialRequestN,
    Payload payload) {
  writer_->writeNewStream(
      streamId_, streamType, initialRequestN, std::move(payload));
}

void StreamStateMachineBase::writePayload(Payload&& payload, bool complete) {
  auto const flags =
      FrameFlags::NEXT | (complete ? FrameFlags::COMPLETE : FrameFlags::EMPTY);
  Frame_PAYLOAD frame{streamId_, flags, std::move(payload)};
  writer_->writePayload(std::move(frame));
}

void StreamStateMachineBase::writeRequestN(uint32_t n) {
  writer_->writeRequestN(Frame_REQUEST_N{streamId_, n});
}

void StreamStateMachineBase::applicationError(std::string msg) {
  writer_->writeError(Frame_ERROR::applicationError(streamId_, std::move(msg)));
}

void StreamStateMachineBase::errorStream(std::string msg) {
  writer_->writeError(Frame_ERROR::invalid(streamId_, std::move(msg)));
  closeStream(StreamCompletionSignal::ERROR);
}

void StreamStateMachineBase::cancelStream() {
  writer_->writeCancel(Frame_CANCEL{streamId_});
}

void StreamStateMachineBase::completeStream() {
  writer_->writePayload(Frame_PAYLOAD::complete(streamId_));
}

void StreamStateMachineBase::closeStream(StreamCompletionSignal signal) {
  writer_->onStreamClosed(streamId_, signal);
  // TODO: set writer_ to nullptr
}
}

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
    Payload payload,
    bool completed) {
  try {
    writer_->writeNewStream(
        streamId_, streamType, initialRequestN, std::move(payload), completed);
  } catch (const std::exception& exn) {
    LOG(ERROR) << "Failed to write new stream: " << exn.what();
    closeStream(StreamCompletionSignal::ERROR);
  }
}

void StreamStateMachineBase::writePayload(Payload&& payload, bool complete) {
  try {
    writer_->writePayload(streamId_, std::move(payload), complete);
  } catch (const std::exception& exn) {
    LOG(ERROR) << "Failed to write payload: " << exn.what();
    closeStream(StreamCompletionSignal::ERROR);
  }
}

void StreamStateMachineBase::writeRequestN(uint32_t n) {
  try {
    writer_->writeRequestN(streamId_, n);
  } catch (const std::exception& exn) {
    LOG(ERROR) << "Failed to write payload: " << exn.what();
    closeStream(StreamCompletionSignal::ERROR);
  }
}

void StreamStateMachineBase::applicationError(std::string errorPayload) {
  // TODO: a bad frame for a stream should not bring down the whole socket
  // https://github.com/ReactiveSocket/reactivesocket-cpp/issues/311
  try {
    writer_->writeCloseStream(
        streamId_,
        StreamCompletionSignal::APPLICATION_ERROR,
        std::move(errorPayload));
  } catch (const std::exception& exn) {
    LOG(ERROR) << "Failed to write payload: " << exn.what();
    closeStream(StreamCompletionSignal::ERROR);
  }
}

void StreamStateMachineBase::errorStream(std::string errorPayload) {
  try {
    writer_->writeCloseStream(
        streamId_, StreamCompletionSignal::ERROR, std::move(errorPayload));
  } catch (const std::exception& exn) {
    LOG(ERROR) << "Failed to write payload: " << exn.what();
  }
  closeStream(StreamCompletionSignal::ERROR);
}

void StreamStateMachineBase::cancelStream() {
  try {
    writer_->writeCloseStream(streamId_, StreamCompletionSignal::CANCEL, "");
  } catch (const std::exception& exn) {
    LOG(ERROR) << "Failed to write payload: " << exn.what();
    closeStream(StreamCompletionSignal::ERROR);
  }
}

void StreamStateMachineBase::completeStream() {
  try {
    writer_->writeCloseStream(streamId_, StreamCompletionSignal::COMPLETE, "");
  } catch (const std::exception& exn) {
    LOG(ERROR) << "Failed to write payload: " << exn.what();
    closeStream(StreamCompletionSignal::ERROR);
  }
}

void StreamStateMachineBase::closeStream(StreamCompletionSignal signal) {
  try {
    writer_->onStreamClosed(streamId_, signal);
    // TODO: set writer_ to nullptr
  } catch (const std::exception& exn) {
    LOG(ERROR) << "Failed to write payload: " << exn.what();
    closeStream(StreamCompletionSignal::ERROR);
  }
}
} // namespace rsocket

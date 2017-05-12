// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/StreamAutomatonBase.h"
#include <folly/io/IOBuf.h>
#include "src/ConnectionAutomaton.h"
#include "src/StreamsHandler.h"

namespace reactivesocket {

void StreamAutomatonBase::handlePayload(Payload&& payload,
                                        bool complete,
                                        bool flagsNext) {
  VLOG(4) << "Unexpected handlePayload";
}

void StreamAutomatonBase::handleRequestN(uint32_t n) {
  VLOG(4) << "Unexpected handleRequestN";
}

void StreamAutomatonBase::handleError(folly::exception_wrapper errorPayload) {
  VLOG(4) << "Unexpected handleError";
}

void StreamAutomatonBase::handleCancel() {
  VLOG(4) << "Unexpected handleCancel";
}

void StreamAutomatonBase::endStream(StreamCompletionSignal) {
  isTerminated_ = true;
}

void StreamAutomatonBase::newStream(
    StreamType streamType,
    uint32_t initialRequestN,
    Payload payload,
    bool completed) {
  writer_->writeNewStream(
      streamId_, streamType, initialRequestN, std::move(payload), completed);
}

void StreamAutomatonBase::writePayload(Payload&& payload, bool complete) {
  writer_->writePayload(streamId_, std::move(payload), complete);
}

void StreamAutomatonBase::writeRequestN(uint32_t n) {
  writer_->writeRequestN(streamId_, n);
}

void StreamAutomatonBase::applicationError(std::string errorPayload) {
  // TODO: a bad frame for a stream should not bring down the whole socket
  // https://github.com/ReactiveSocket/reactivesocket-cpp/issues/311
  writer_->writeCloseStream(
      streamId_,
      StreamCompletionSignal::APPLICATION_ERROR,
      Payload(std::move(errorPayload)));
  closeStream(StreamCompletionSignal::APPLICATION_ERROR);
}

void StreamAutomatonBase::errorStream(std::string errorPayload) {
  writer_->writeCloseStream(
      streamId_,
      StreamCompletionSignal::ERROR,
      Payload(std::move(errorPayload)));
  closeStream(StreamCompletionSignal::ERROR);
}

void StreamAutomatonBase::cancelStream() {
  writer_->writeCloseStream(
      streamId_, StreamCompletionSignal::CANCEL, Payload());
  closeStream(StreamCompletionSignal::CANCEL);
}

void StreamAutomatonBase::completeStream() {
  writer_->writeCloseStream(
      streamId_, StreamCompletionSignal::COMPLETE, Payload());
  closeStream(StreamCompletionSignal::COMPLETE);
}

void StreamAutomatonBase::closeStream(StreamCompletionSignal signal) {
  writer_->onStreamClosed(streamId_, signal);
  // TODO: set writer_ to nullptr
}
} // reactivesocket

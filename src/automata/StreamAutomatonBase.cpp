// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/StreamAutomatonBase.h"
#include <folly/io/IOBuf.h>
#include "src/ConnectionAutomaton.h"
#include "src/Frame.h"
#include "src/StreamsHandler.h"

namespace reactivesocket {

void StreamAutomatonBase::onNextFrame(std::unique_ptr<folly::IOBuf> payload) {
  DCHECK(payload);

  auto type = writer_->frameSerializer().peekFrameType(*payload);
  switch (type) {
    case FrameType::REQUEST_CHANNEL:
      deserializeAndDispatch<Frame_REQUEST_CHANNEL>(std::move(payload));
      return;
    case FrameType::REQUEST_N:
      deserializeAndDispatch<Frame_REQUEST_N>(std::move(payload));
      return;
    case FrameType::REQUEST_RESPONSE:
      deserializeAndDispatch<Frame_REQUEST_RESPONSE>(std::move(payload));
      return;
    case FrameType::CANCEL:
      deserializeAndDispatch<Frame_CANCEL>(std::move(payload));
      return;
    case FrameType::PAYLOAD:
      deserializeAndDispatch<Frame_PAYLOAD>(std::move(payload));
      return;
    case FrameType::ERROR:
      deserializeAndDispatch<Frame_ERROR>(std::move(payload));
      return;

    case FrameType::RESERVED:
    case FrameType::SETUP:
    case FrameType::LEASE:
    case FrameType::KEEPALIVE:
    case FrameType::REQUEST_FNF:
    case FrameType::REQUEST_STREAM:
    case FrameType::METADATA_PUSH:
    case FrameType::RESUME:
    case FrameType::RESUME_OK:
    case FrameType::EXT:
    default:
      onUnknownFrame();
      return;
  }
}

template <class Frame>
void StreamAutomatonBase::deserializeAndDispatch(
    std::unique_ptr<folly::IOBuf> payload) {
  Frame frame;
  if (writer_->frameSerializer().deserializeFrom(frame, std::move(payload))) {
    onNextFrame(std::move(frame));
  } else {
    onBadFrame();
  }
}

template <typename T>
static void onUnexpectedFrame(const T& frame) {
  VLOG(4) << "Unexpected frame, ignoring: " << frame;
}

void StreamAutomatonBase::endStream(StreamCompletionSignal) {
  isTerminated_ = true;
}

void StreamAutomatonBase::onNextFrame(Frame_REQUEST_STREAM&& f) {
  onUnexpectedFrame(f);
}

void StreamAutomatonBase::onNextFrame(Frame_REQUEST_CHANNEL&& f) {
  onUnexpectedFrame(f);
}

void StreamAutomatonBase::onNextFrame(Frame_REQUEST_RESPONSE&& f) {
  onUnexpectedFrame(f);
}

void StreamAutomatonBase::onNextFrame(Frame_REQUEST_N&& f) {
  onUnexpectedFrame(f);
}

void StreamAutomatonBase::onNextFrame(Frame_CANCEL&& f) {
  onUnexpectedFrame(f);
}

void StreamAutomatonBase::onNextFrame(Frame_PAYLOAD&& f) {
  onUnexpectedFrame(f);
}

void StreamAutomatonBase::onNextFrame(Frame_ERROR&& f) {
  onUnexpectedFrame(f);
}

void StreamAutomatonBase::onBadFrame() {
  errorStream("bad frame");
}

void StreamAutomatonBase::onUnknownFrame() {
  // because of compatibility with future frame types we will just ignore
  // unknown frames
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

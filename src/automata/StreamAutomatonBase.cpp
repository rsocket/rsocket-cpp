// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/StreamAutomatonBase.h"
#include "src/ConnectionAutomaton.h"
#include "src/Frame.h"

namespace reactivesocket {

namespace {

template <typename T>
void onUnexpectedFrame(const T& frame) {
  VLOG(4) << "Unexpected frame, ignoring: " << frame;
}
}

void StreamAutomatonBase::endStream(StreamCompletionSignal) {
  isTerminated_ = true;
}

void StreamAutomatonBase::onNextFrame(Frame_REQUEST_STREAM&& f) {
  onUnexpectedFrame(f);
}

void StreamAutomatonBase::onNextFrame(Frame_REQUEST_SUB&& f) {
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

void StreamAutomatonBase::onNextFrame(Frame_RESPONSE&& f) {
  onUnexpectedFrame(f);
}

void StreamAutomatonBase::onNextFrame(Frame_ERROR&& f) {
  onUnexpectedFrame(f);
}

void StreamAutomatonBase::onBadFrame() {
  connection_->closeWithError(Frame_ERROR::invalid(streamId_, "bad frame"));
}

void StreamAutomatonBase::onUnknownFrame() {
  // because of compatibility with future frame types we will just ignore
  // unknown frames
}
} // reactivesocket

// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/statemachine/StreamRequester.h"

namespace rsocket {

void StreamRequester::request(int64_t n) noexcept {
  if (n == 0) {
    return;
  }

  if(!requested_) {
    requested_ = true;

    auto initialN =
        n > Frame_REQUEST_N::kMaxRequestN ? Frame_REQUEST_N::kMaxRequestN : n;
    auto remainingN = n > Frame_REQUEST_N::kMaxRequestN
        ? n - Frame_REQUEST_N::kMaxRequestN
        : 0;

    // Send as much as possible with the initial request.
    CHECK_GE(Frame_REQUEST_N::kMaxRequestN, initialN);

    // We must inform ConsumerBase about an implicit allowance we have
    // requested from the remote end.
    addImplicitAllowance(initialN);
    newStream(
        StreamType::STREAM,
        static_cast<uint32_t>(initialN),
        std::move(initialPayload_));

    // Pump the remaining allowance into the ConsumerBase _after_ sending the
    // initial request.
    if (remainingN) {
      Base::generateRequest(remainingN);
    }
    return;
  }

  checkConsumerRequest();
  ConsumerBase::generateRequest(n);
}

void StreamRequester::cancel() noexcept {
  if (requested_) {
    cancelConsumer();
    cancelStream();
  }
  closeStream(StreamCompletionSignal::CANCEL);
}

void StreamRequester::endStream(StreamCompletionSignal signal) {
  ConsumerBase::endStream(signal);
}

void StreamRequester::handlePayload(
    Payload&& payload,
    bool complete,
    bool next) {
  CHECK(requested_);
  processPayload(std::move(payload), next);

  if (complete) {
    completeConsumer();
    closeStream(StreamCompletionSignal::COMPLETE);
  }
}

void StreamRequester::handleError(folly::exception_wrapper errorPayload) {
  CHECK(requested_);
  errorConsumer(std::move(errorPayload));
  closeStream(StreamCompletionSignal::ERROR);
}
}

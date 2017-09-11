// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/statemachine/StreamRequester.h"

namespace rsocket {

void StreamRequester::setRequested(size_t n) {
  VLOG(3) << "Setting allowance to " << n;
  requested_ = true;
  addImplicitAllowance(n);
}

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
      generateRequest(remainingN);
    }
    return;
  }

  generateRequest(n);
}

void StreamRequester::cancel() noexcept {
  VLOG(5) << "StreamRequester::cancel(requested_=" << requested_ << ")";
  if (requested_) {
    cancelConsumer();
    cancelStream();
  }
  closeStream(StreamCompletionSignal::CANCEL);
}

void StreamRequester::endStream(StreamCompletionSignal signal) {
  VLOG(5) << "StreamRequester::endStream()";
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

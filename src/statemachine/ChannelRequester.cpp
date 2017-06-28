// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/statemachine/ChannelRequester.h"
#include "yarpl/utils/ExceptionString.h"

namespace rsocket {

using namespace yarpl;
using namespace yarpl::flowable;

ChannelRequester::ChannelRequester(const ConsumerBase::Parameters& params)
    : ConsumerBase(params), PublisherBase(/*initialRequestN=*/1) {}

void ChannelRequester::onSubscribe(
    Reference<Subscription> subscription) noexcept {
  CHECK(!requested_);
  publisherSubscribe(std::move(subscription));
}

void ChannelRequester::onNext(Payload request) noexcept {
  if(!requested_) {
    requested_ = true;

    size_t initialN = initialResponseAllowance_.drainWithLimit(
        Frame_REQUEST_N::kMaxRequestN);
    size_t remainingN = initialResponseAllowance_.drain();
    // Send as much as possible with the initial request.
    CHECK_GE(Frame_REQUEST_N::kMaxRequestN, initialN);
    newStream(
        StreamType::CHANNEL,
        static_cast<uint32_t>(initialN),
        std::move(request),
        false);
    // We must inform ConsumerBase about an implicit allowance we have
    // requested from the remote end.
    ConsumerBase::addImplicitAllowance(initialN);
    // Pump the remaining allowance into the ConsumerBase _after_ sending the
    // initial request.
    if (remainingN) {
      ConsumerBase::generateRequest(remainingN);
    }
    return;
  }

  checkPublisherOnNext();
  writePayload(std::move(request), false);
}

// TODO: consolidate code in onCompleteImpl, onErrorImpl, cancelImpl
void ChannelRequester::onComplete() noexcept {
  if (!requested_) {
    closeStream(StreamCompletionSignal::CANCEL);
    return;
  }
  publisherComplete();
  completeStream();
  tryCompleteChannel();
}

void ChannelRequester::onError(std::exception_ptr ex) noexcept {
  if (!requested_) {
    closeStream(StreamCompletionSignal::CANCEL);
    return;
  }
  publisherComplete();
  applicationError(yarpl::exceptionStr(ex));
  tryCompleteChannel();
}

void ChannelRequester::request(int64_t n) noexcept {
  if (!requested_) {
    // The initial request has not been sent out yet, hence we must accumulate
    // the unsynchronised allowance, portion of which will be sent out with
    // the initial request frame, and the rest will be dispatched via
    // ConsumerBase:request (ultimately by sending REQUEST_N frames).
    initialResponseAllowance_.release(n);
    return;
  }
  checkConsumerRequest();
  ConsumerBase::generateRequest(n);
}

void ChannelRequester::cancel() noexcept {
  if (!requested_) {
    closeStream(StreamCompletionSignal::CANCEL);
    return;
  }
  cancelConsumer();
  cancelStream();
  tryCompleteChannel();
}

void ChannelRequester::endStream(StreamCompletionSignal signal) {
  terminatePublisher();
  ConsumerBase::endStream(signal);
}

void ChannelRequester::tryCompleteChannel() {
  if (publisherClosed() && consumerClosed()) {
    closeStream(StreamCompletionSignal::COMPLETE);
  }
}

void ChannelRequester::handlePayload(
    Payload&& payload,
    bool complete,
    bool next) {
  CHECK(requested_);
  processPayload(std::move(payload), next);

  if (complete) {
    completeConsumer();
    tryCompleteChannel();
  }
}

void ChannelRequester::handleError(folly::exception_wrapper ex) {
  CHECK(requested_);
  errorConsumer(std::move(ex));
  tryCompleteChannel();
}

void ChannelRequester::handleRequestN(uint32_t n) {
  CHECK(requested_);
  PublisherBase::processRequestN(n);
}

void ChannelRequester::handleCancel() {
  CHECK(requested_);
  publisherComplete();
  tryCompleteChannel();
}
} // reactivesocket

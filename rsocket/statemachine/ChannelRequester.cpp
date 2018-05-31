// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/statemachine/ChannelRequester.h"

namespace rsocket {

void ChannelRequester::onSubscribe(
    std::shared_ptr<yarpl::flowable::Subscription> subscription) {
  CHECK(!requested_);
  publisherSubscribe(std::move(subscription));

  if (hasInitialRequest_) {
    initStream(std::move(request_));
  }
}

void ChannelRequester::onNext(Payload request) {
  if (!requested_) {
    initStream(std::move(request));
    return;
  }

  if (!publisherClosed()) {
    writePayload(std::move(request));
  }
}

// TODO: consolidate code in onCompleteImpl, onErrorImpl, cancelImpl
void ChannelRequester::onComplete() {
  if (!requested_) {
    endStream(StreamCompletionSignal::CANCEL);
    removeFromWriter();
    return;
  }
  if (!publisherClosed()) {
    publisherComplete();
    writeComplete();
    tryCompleteChannel();
  }
}

void ChannelRequester::onError(folly::exception_wrapper ex) {
  if (!requested_) {
    endStream(StreamCompletionSignal::CANCEL);
    removeFromWriter();
    return;
  }
  if (!publisherClosed()) {
    publisherComplete();
    endStream(StreamCompletionSignal::ERROR);
    writeApplicationError(ex.get_exception()->what());
    tryCompleteChannel();
  }
}

void ChannelRequester::request(int64_t n) {
  if (!requested_) {
    // The initial request has not been sent out yet, hence we must accumulate
    // the unsynchronised allowance, portion of which will be sent out with
    // the initial request frame, and the rest will be dispatched via
    // ConsumerBase:request (ultimately by sending REQUEST_N frames).
    initialResponseAllowance_.add(n);
    return;
  }
  ConsumerBase::generateRequest(n);
}

void ChannelRequester::cancel() {
  if (!requested_) {
    endStream(StreamCompletionSignal::CANCEL);
    removeFromWriter();
    return;
  }
  cancelConsumer();
  writeCancel();
  tryCompleteChannel();
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

void ChannelRequester::handleRequestN(uint32_t n) {
  CHECK(requested_);
  PublisherBase::processRequestN(n);
}

void ChannelRequester::handleError(folly::exception_wrapper ex) {
  CHECK(requested_);
  errorConsumer(std::move(ex));
  terminatePublisher();
}

void ChannelRequester::handleCancel() {
  CHECK(requested_);
  terminatePublisher();
  tryCompleteChannel();
}

void ChannelRequester::endStream(StreamCompletionSignal signal) {
  terminatePublisher();
  ConsumerBase::endStream(signal);
}

void ChannelRequester::initStream(Payload&& request) {
  requested_ = true;

  const size_t initialN =
      initialResponseAllowance_.consumeUpTo(Frame_REQUEST_N::kMaxRequestN);
  const size_t remainingN = initialResponseAllowance_.consumeAll();

  // Send as much as possible with the initial request.
  CHECK_GE(Frame_REQUEST_N::kMaxRequestN, initialN);
  newStream(
      StreamType::CHANNEL, static_cast<uint32_t>(initialN), std::move(request));
  // We must inform ConsumerBase about an implicit allowance we have
  // requested from the remote end.
  ConsumerBase::addImplicitAllowance(initialN);
  // Pump the remaining allowance into the ConsumerBase _after_ sending the
  // initial request.
  if (remainingN) {
    ConsumerBase::generateRequest(remainingN);
  }
}

void ChannelRequester::tryCompleteChannel() {
  if (publisherClosed() && consumerClosed()) {
    endStream(StreamCompletionSignal::COMPLETE);
    removeFromWriter();
  }
}

} // namespace rsocket

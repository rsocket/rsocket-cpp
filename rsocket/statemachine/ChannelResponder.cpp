// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/statemachine/ChannelResponder.h"

namespace rsocket {

void ChannelResponder::onSubscribe(
    std::shared_ptr<yarpl::flowable::Subscription> subscription) {
  publisherSubscribe(std::move(subscription));
}

void ChannelResponder::onNext(Payload response) {
  if (!publisherClosed()) {
    writePayload(std::move(response));
  }
}

void ChannelResponder::onComplete() {
  if (!publisherClosed()) {
    publisherComplete();
    writeComplete();
    tryCompleteChannel();
  }
}

void ChannelResponder::onError(folly::exception_wrapper ex) {
  if (!publisherClosed()) {
    publisherComplete();
    endStream(StreamCompletionSignal::ERROR);
    writeApplicationError(ex.get_exception()->what());
    tryCompleteChannel();
  }
}

void ChannelResponder::request(int64_t n) {
  ConsumerBase::generateRequest(n);
}

void ChannelResponder::cancel() {
  cancelConsumer();
  writeCancel();
  tryCompleteChannel();
}

void ChannelResponder::handlePayload(
    Payload&& payload,
    bool complete,
    bool next) {
  processPayload(std::move(payload), next);

  if (complete) {
    completeConsumer();
    tryCompleteChannel();
  }
}

void ChannelResponder::handleRequestN(uint32_t n) {
  processRequestN(n);
}

void ChannelResponder::handleError(folly::exception_wrapper ex) {
  errorConsumer(std::move(ex));
  terminatePublisher();
}

void ChannelResponder::handleCancel() {
  terminatePublisher();
  tryCompleteChannel();
}

void ChannelResponder::endStream(StreamCompletionSignal signal) {
  terminatePublisher();
  ConsumerBase::endStream(signal);
}

void ChannelResponder::tryCompleteChannel() {
  if (publisherClosed() && consumerClosed()) {
    endStream(StreamCompletionSignal::COMPLETE);
    removeFromWriter();
  }
}

} // namespace rsocket

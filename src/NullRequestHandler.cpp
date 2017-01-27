// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/NullRequestHandler.h"
#include "src/Stats.h"

namespace reactivesocket {

template class NullSubscriberT<Payload>;

void NullSubscription::request(size_t /*n*/) noexcept {}

void NullSubscription::cancel() noexcept {}

std::shared_ptr<Subscriber<Payload>> NullRequestHandler::handleRequestChannel(
    Payload /*request*/,
    StreamId /*streamId*/,
    const std::shared_ptr<Subscriber<Payload>>& response) noexcept {
  // TODO(lehecka): get rid of onSubscribe call
  response->onSubscribe(std::make_shared<NullSubscription>());
  response->onError(std::runtime_error("NullRequestHandler"));
  return std::make_shared<NullSubscriber>();
}

void NullRequestHandler::handleRequestStream(
    Payload /*request*/,
    StreamId /*streamId*/,
    const std::shared_ptr<Subscriber<Payload>>& response) noexcept {
  // TODO(lehecka): get rid of onSubscribe call
  response->onSubscribe(std::make_shared<NullSubscription>());
  response->onError(std::runtime_error("NullRequestHandler"));
}

void NullRequestHandler::handleRequestSubscription(
    Payload /*request*/,
    StreamId /*streamId*/,
    const std::shared_ptr<Subscriber<Payload>>& response) noexcept {
  // TODO(lehecka): get rid of onSubscribe call
  response->onSubscribe(std::make_shared<NullSubscription>());
  response->onError(std::runtime_error("NullRequestHandler"));
}

void NullRequestHandler::handleRequestResponse(
    Payload /*request*/,
    StreamId /*streamId*/,
    const std::shared_ptr<Subscriber<Payload>>& response) noexcept {
  response->onSubscribe(std::make_shared<NullSubscription>());
  response->onError(std::runtime_error("NullRequestHandler"));
}

void NullRequestHandler::handleFireAndForgetRequest(
    Payload /*request*/,
    StreamId /*streamId*/) noexcept {}

void NullRequestHandler::handleMetadataPush(
    std::unique_ptr<folly::IOBuf> /*request*/) noexcept {}

std::shared_ptr<StreamState> NullRequestHandler::handleSetupPayload(
    ReactiveSocket& socket,
    ConnectionSetupPayload /*request*/) noexcept {
  return std::make_shared<StreamState>(Stats::noop());
}

bool NullRequestHandler::handleResume(
    ReactiveSocket& socket,
    const ResumeIdentificationToken& /*token*/,
    ResumePosition /*position*/) noexcept {
  return false;
}

void NullRequestHandler::handleCleanResume(
    std::shared_ptr<Subscription> /* response */) noexcept {}

void NullRequestHandler::handleDirtyResume(
    std::shared_ptr<Subscription> /* response */) noexcept {}

void NullRequestHandler::onSubscriptionPaused(
    const std::shared_ptr<Subscription>&) noexcept {}
void NullRequestHandler::onSubscriptionResumed(
    const std::shared_ptr<Subscription>&) noexcept {}
void NullRequestHandler::onSubscriberPaused(
    const std::shared_ptr<Subscriber<Payload>>&) noexcept {}
void NullRequestHandler::onSubscriberResumed(
    const std::shared_ptr<Subscriber<Payload>>&) noexcept {}

} // reactivesocket

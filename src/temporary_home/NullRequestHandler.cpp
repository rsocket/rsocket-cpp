// Copyright 2004-present Facebook. All Rights Reserved.

#include "NullRequestHandler.h"

namespace reactivesocket {

using namespace yarpl;
using namespace yarpl::flowable;

template class NullSubscriberT<Payload>;

void NullSubscription::request(int64_t /*n*/) noexcept {}

void NullSubscription::cancel() noexcept {}

Reference<Subscriber<Payload>> NullRequestHandler::handleRequestChannel(
    Payload /*request*/,
    StreamId /*streamId*/,
    const Reference<Subscriber<Payload>>& response) noexcept {
  // TODO(lehecka): get rid of onSubscribe call
  response->onSubscribe(make_ref<NullSubscription>());
  response->onError(std::make_exception_ptr(std::runtime_error("NullRequestHandler")));
  return make_ref<NullSubscriber>();
}

void NullRequestHandler::handleRequestStream(
    Payload /*request*/,
    StreamId /*streamId*/,
    const Reference<Subscriber<Payload>>& response) noexcept {
  // TODO(lehecka): get rid of onSubscribe call
  response->onSubscribe(make_ref<NullSubscription>());
  response->onError(std::make_exception_ptr(std::runtime_error("NullRequestHandler")));
}

void NullRequestHandler::handleRequestResponse(
    Payload /*request*/,
    StreamId /*streamId*/,
    const Reference<Subscriber<Payload>>& response) noexcept {
  response->onSubscribe(make_ref<NullSubscription>());
  response->onError(std::make_exception_ptr(std::runtime_error("NullRequestHandler")));
}

void NullRequestHandler::handleFireAndForgetRequest(
    Payload /*request*/,
    StreamId /*streamId*/) noexcept {}

void NullRequestHandler::handleMetadataPush(
    std::unique_ptr<folly::IOBuf> /*request*/) noexcept {}

std::shared_ptr<StreamState> NullRequestHandler::handleSetupPayload(
    ReactiveSocket& socket,
    ConnectionSetupPayload /*request*/) noexcept {
  return nullptr;
}

bool NullRequestHandler::handleResume(
    ReactiveSocket& socket,
    ResumeParameters) noexcept {
  return false;
}

void NullRequestHandler::handleCleanResume(
    Reference<Subscription> /* response */) noexcept {}

void NullRequestHandler::handleDirtyResume(
    Reference<Subscription> /* response */) noexcept {}

void NullRequestHandler::onSubscriptionPaused(
    const Reference<Subscription>&) noexcept {}
void NullRequestHandler::onSubscriptionResumed(
    const Reference<Subscription>&) noexcept {}
void NullRequestHandler::onSubscriberPaused(
    const Reference<Subscriber<Payload>>&) noexcept {}
void NullRequestHandler::onSubscriberResumed(
    const Reference<Subscriber<Payload>>&) noexcept {}

} // reactivesocket

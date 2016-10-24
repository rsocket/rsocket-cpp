// Copyright 2004-present Facebook. All Rights Reserved.

#include "NullRequestHandler.h"

#include <folly/ExceptionWrapper.h>

namespace reactivesocket {

void NullSubscriber::onSubscribe(std::shared_ptr<Subscription> subscription) {
  subscription->cancel();
}

void NullSubscriber::onNext(Payload /*element*/) {}

void NullSubscriber::onComplete() {}

void NullSubscriber::onError(folly::exception_wrapper /*ex*/) {}

void NullSubscription::request(size_t /*n*/){};

void NullSubscription::cancel() {}

std::shared_ptr<Subscriber<Payload>> NullRequestHandler::handleRequestChannel(
    Payload /*request*/,
    const std::shared_ptr<Subscriber<Payload>>& response) {
  // TODO(lehecka): get rid of onSubscribe call
  response->onSubscribe(std::make_shared<NullSubscription>());
  response->onError(std::runtime_error("NullRequestHandler"));
  return std::make_shared<NullSubscriber>();
}

void NullRequestHandler::handleRequestStream(
    Payload /*request*/,
    const std::shared_ptr<Subscriber<Payload>>& response) {
  // TODO(lehecka): get rid of onSubscribe call
  response->onSubscribe(std::make_shared<NullSubscription>());
  response->onError(std::runtime_error("NullRequestHandler"));
}

void NullRequestHandler::handleRequestSubscription(
    Payload /*request*/,
    const std::shared_ptr<Subscriber<Payload>>& response) {
  // TODO(lehecka): get rid of onSubscribe call
  response->onSubscribe(std::make_shared<NullSubscription>());
  response->onError(std::runtime_error("NullRequestHandler"));
}

void NullRequestHandler::handleRequestResponse(
    Payload /*request*/,
    const std::shared_ptr<Subscriber<Payload>>& response) {
  response->onSubscribe(std::make_shared<NullSubscription>());
  response->onError(std::runtime_error("NullRequestHandler"));
}

void NullRequestHandler::handleFireAndForgetRequest(Payload /*request*/) {}

void NullRequestHandler::handleMetadataPush(
    std::unique_ptr<folly::IOBuf> /*request*/) {}

void NullRequestHandler::handleSetupPayload(
    ConnectionSetupPayload /*request*/) {}
} // reactivesocket

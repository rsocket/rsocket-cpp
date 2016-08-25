// Copyright 2004-present Facebook. All Rights Reserved.

#include "NullRequestHandler.h"

#include <folly/ExceptionWrapper.h>
#include "src/mixins/MemoryMixin.h"

namespace reactivesocket {

void NullSubscriber::onSubscribe(Subscription& subscription) {
  subscription.cancel();
}

void NullSubscriber::onNext(Payload /*element*/) {}

void NullSubscriber::onComplete() {}

void NullSubscriber::onError(folly::exception_wrapper /*ex*/) {}

void NullSubscription::request(size_t /*n*/){};

void NullSubscription::cancel() {}

Subscriber<Payload>& NullRequestHandler::handleRequestChannel(
    Payload /*request*/,
    Subscriber<Payload>& response) {
  // TODO(lehecka): get rid of onSubscribe call
  response.onSubscribe(createManagedInstance<NullSubscription>());
  response.onError(std::runtime_error("NullRequestHandler"));
  return createManagedInstance<NullSubscriber>();
}

void NullRequestHandler::handleRequestStream(
    Payload /*request*/,
    Subscriber<Payload>& response) {
  // TODO(lehecka): get rid of onSubscribe call
  response.onSubscribe(createManagedInstance<NullSubscription>());
  response.onError(std::runtime_error("NullRequestHandler"));
}

void NullRequestHandler::handleRequestSubscription(
    Payload /*request*/,
    Subscriber<Payload>& response) {
  // TODO(lehecka): get rid of onSubscribe call
  response.onSubscribe(createManagedInstance<NullSubscription>());
  response.onError(std::runtime_error("NullRequestHandler"));
}

void NullRequestHandler::handleFireAndForgetRequest(Payload /*request*/) {}

void NullRequestHandler::handleMetadataPush(
    std::unique_ptr<folly::IOBuf> /*request*/) {}

void NullRequestHandler::handleSetupPayload(
    ConnectionSetupPayload /*request*/) {}
}

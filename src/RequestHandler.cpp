// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/RequestHandler.h"

namespace reactivesocket {

std::shared_ptr<Subscriber<Payload>> RequestHandler::onRequestChannel(
    Payload request,
    SubscriberFactory& subscriberFactory) {
  return handleRequestChannel(
      std::move(request), subscriberFactory.createSubscriber());
}

/// Handles a new Stream requested by the other end.
void RequestHandler::onRequestStream(
    Payload request,
    SubscriberFactory& subscriberFactory) {
  handleRequestStream(std::move(request), subscriberFactory.createSubscriber());
}

/// Handles a new inbound Subscription requested by the other end.
void RequestHandler::onRequestSubscription(
    Payload request,
    SubscriberFactory& subscriberFactory) {
  handleRequestSubscription(
      std::move(request), subscriberFactory.createSubscriber());
}

/// Handles a new inbound RequestResponse requested by the other end.
void RequestHandler::onRequestResponse(
    Payload request,
    SubscriberFactory& subscriberFactory) {
  handleRequestResponse(
      std::move(request), subscriberFactory.createSubscriber());
}
}

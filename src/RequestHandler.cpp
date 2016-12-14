// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/RequestHandler.h"

namespace reactivesocket {

std::shared_ptr<Subscriber<Payload>> RequestHandler::onRequestChannel(
    Payload request,
    StreamId streamId,
    SubscriberFactory& subscriberFactory) {
  return handleRequestChannel(
      std::move(request), streamId, subscriberFactory.createSubscriber());
}

/// Handles a new Stream requested by the other end.
void RequestHandler::onRequestStream(
    Payload request,
    StreamId streamId,
    SubscriberFactory& subscriberFactory) {
  handleRequestStream(
      std::move(request), streamId, subscriberFactory.createSubscriber());
}

/// Handles a new inbound Subscription requested by the other end.
void RequestHandler::onRequestSubscription(
    Payload request,
    StreamId streamId,
    SubscriberFactory& subscriberFactory) {
  handleRequestSubscription(
      std::move(request), streamId, subscriberFactory.createSubscriber());
}

/// Handles a new inbound RequestResponse requested by the other end.
void RequestHandler::onRequestResponse(
    Payload request,
    StreamId streamId,
    SubscriberFactory& subscriberFactory) {
  handleRequestResponse(
      std::move(request), streamId, subscriberFactory.createSubscriber());
}
}

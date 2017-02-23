// Copyright 2004-present Facebook. All Rights Reserved.

#include "ServerRequestHandler.h"
#include "IntStreamSubscription.h"

using namespace ::reactivesocket;

/// Handles a new inbound Stream requested by the other end.
void ServerRequestHandler::handleRequestStream(
    Payload request,
    StreamId streamId,
    const std::shared_ptr<Subscriber<Payload>>& response) noexcept {
  LOG(INFO) << "ServerRequestHandler.handleRequestStream " << request;

  response->onSubscribe(std::make_shared<IntStreamSubscription>(response, 10));
}

std::shared_ptr<StreamState> ServerRequestHandler::handleSetupPayload(
    ReactiveSocket& socket,
    ConnectionSetupPayload request) noexcept {
  LOG(INFO) << "ServerRequestHandler.handleSetupPayload " << request;
  return std::make_shared<StreamState>(Stats::noop());
}
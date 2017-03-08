// Copyright 2004-present Facebook. All Rights Reserved.

#include "TextRequestHandler.h"
#include <string>
#include "ConditionalRequestSubscription.h"

using namespace ::reactivesocket;

/// Handles a new inbound Stream requested by the other end.
void TextRequestHandler::handleRequestStream(
    Payload request,
    StreamId streamId,
    const std::shared_ptr<Subscriber<Payload>>& response) noexcept {
  LOG(INFO) << "TextRequestHandler.handleRequestStream " << request;

  // string from payload data
  auto pds = request.moveDataToString();
  auto requestString = std::string(pds, request.data->length());

  response->onSubscribe(std::make_shared<ConditionalRequestSubscription>(
      response, requestString, 10));
}

std::shared_ptr<StreamState> TextRequestHandler::handleSetupPayload(
    ReactiveSocket& socket,
    ConnectionSetupPayload request) noexcept {
  LOG(INFO) << "TextRequestHandler.handleSetupPayload " << request;
  // TODO what should this do?
  return nullptr;
}

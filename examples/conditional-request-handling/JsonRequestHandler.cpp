// Copyright 2004-present Facebook. All Rights Reserved.

#include "JsonRequestHandler.h"
#include <string>
#include "ConditionalRequestSubscription.h"

using namespace ::reactivesocket;

/// Handles a new inbound Stream requested by the other end.
void JsonRequestHandler::handleRequestStream(
    Payload request,
    StreamId streamId,
    const std::shared_ptr<Subscriber<Payload>>& response) noexcept {
  LOG(INFO) << "JsonRequestHandler.handleRequestStream " << request;

  // string from payload data
  auto pds = request.moveDataToString();
  auto requestString = std::string(pds, request.data->length());

  response->onSubscribe(std::make_shared<ConditionalRequestSubscription>(
      response, requestString, 10));
}

std::shared_ptr<StreamState> JsonRequestHandler::handleSetupPayload(
    ReactiveSocket& socket,
    ConnectionSetupPayload request) noexcept {
  LOG(INFO) << "JsonRequestHandler.handleSetupPayload " << request;
  // TODO what should this do?
  return nullptr;
}

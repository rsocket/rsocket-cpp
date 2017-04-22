// Copyright 2004-present Facebook. All Rights Reserved.

#include "HelloStreamRequestHandler.h"
#include <string>
#include "HelloStreamSubscription.h"

using namespace ::reactivesocket;

namespace rsocket {
namespace tests {
/// Handles a new inbound Stream requested by the other end.
void HelloStreamRequestHandler::handleRequestStream(
    Payload request,
    StreamId streamId,
    const std::shared_ptr<Subscriber<Payload>>& response) noexcept {
  LOG(INFO) << "HelloStreamRequestHandler.handleRequestStream " << request;

  // string from payload data
  const char* p = reinterpret_cast<const char*>(request.data->data());
  auto requestString = std::string(p, request.data->length());

  response->onSubscribe(
      std::make_shared<HelloStreamSubscription>(response, requestString, 10));
}

std::shared_ptr<StreamState> HelloStreamRequestHandler::handleSetupPayload(
    ReactiveSocket& socket,
    ConnectionSetupPayload request) noexcept {
  LOG(INFO) << "HelloStreamRequestHandler.handleSetupPayload " << request;
  // TODO what should this do?
  return nullptr;
}
}
}

// Copyright 2004-present Facebook. All Rights Reserved.

#include "test/handlers/HelloServiceHandler.h"
#include "test/handlers/HelloStreamRequestHandler.h"

namespace rsocket {
namespace tests {

folly::Expected<RSocketConnectionParams, RSocketException>
HelloServiceHandler::onNewSetup(const SetupParameters&) {
  return RSocketConnectionParams(
      std::make_shared<rsocket::tests::HelloStreamRequestHandler>(),
      RSocketStats::noop(),
      connectionEvents_);
}

void HelloServiceHandler::onNewRSocketState(
    std::shared_ptr<RSocketServerState> state,
    ResumeIdentificationToken token) {
  store_.lock()->insert({token, std::move(state)});
}

folly::Expected<std::shared_ptr<RSocketServerState>, RSocketException>
HelloServiceHandler::onResume(ResumeIdentificationToken token) {
  auto itr = store_->find(token);
  CHECK(itr != store_->end());
  return itr->second;
}

} // namespace tests
} // namespace rsocket

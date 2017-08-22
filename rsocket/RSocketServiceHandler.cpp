// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/RSocketServiceHandler.h"

namespace rsocket {

void RSocketServiceHandler::onNewRSocketState(
    std::shared_ptr<RSocketServerState>,
    ResumeIdentificationToken) {}

folly::Expected<std::shared_ptr<RSocketServerState>, RSocketException>
RSocketServiceHandler::onResume(ResumeIdentificationToken) {
  return folly::makeUnexpected(RSocketException("No ServerState"));
}

bool RSocketServiceHandler::canResume(
    const std::vector<StreamId>& /* cleanStreamIds */,
    const std::vector<StreamId>& /* dirtyStreamIds */,
    ResumeIdentificationToken) {
  return true;
}

std::shared_ptr<RSocketServiceHandler> RSocketServiceHandler::create(
    OnNewSetupFn onNewSetupFn) {
  class ServiceHandler : public RSocketServiceHandler {
   public:
    explicit ServiceHandler(OnNewSetupFn fn) : onNewSetupFn_(std::move(fn)) {}
    folly::Expected<RSocketConnectionParams, RSocketException> onNewSetup(
        const SetupParameters& setupParameters) override {
      try {
        return RSocketConnectionParams(onNewSetupFn_(setupParameters));
      } catch(const std::exception& e) {
        return folly::Unexpected<RSocketException>(
            ConnectionException(e.what()));
      }
    }

   private:
    OnNewSetupFn onNewSetupFn_;
  };
  return std::make_shared<ServiceHandler>(std::move(onNewSetupFn));
}
}

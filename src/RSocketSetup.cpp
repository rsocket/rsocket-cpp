// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/RSocketSetup.h"
#include "src/RSocketParameters.h"
#include "src/internal/RSocketConnectionManager.h"
#include "src/internal/ScheduledRSocketResponder.h"
#include "src/framing/FrameTransport.h"
#include "src/statemachine/RSocketStateMachine.h"
#include "src/RSocketRequester.h"
#include "src/RSocketErrors.h"
#include "src/RSocketStats.h"

namespace rsocket {

RSocketSetup::RSocketSetup(
    yarpl::Reference<FrameTransport> frameTransport,
    SetupParameters setupParams,
    folly::EventBase& eventBase,
    RSocketConnectionManager& connectionManager)
  : frameTransport_(std::move(frameTransport)),
    setupParams_(std::move(setupParams)),
    eventBase_(eventBase),
    connectionManager_(connectionManager) {}

RSocketSetup::~RSocketSetup() {
  if (frameTransport_) {
    // this instance was ignored and no RSocket instance was created from it
    // we will just close the transport
    frameTransport_->closeWithError(std::runtime_error("ignored connection"));
  }
}

std::unique_ptr<RSocketRequester> RSocketSetup::createRSocketRequester(
    std::shared_ptr<RSocketResponder> requestResponder,
    std::shared_ptr<RSocketStats> stats,
    std::shared_ptr<RSocketNetworkStats> networkStats) {
  auto rs = createRSocketStateMachine(std::move(requestResponder), std::move(stats), std::move(networkStats));
  return std::make_unique<RSocketRequester>(std::move(rs), eventBase_);
}

void RSocketSetup::createRSocket(
    std::shared_ptr<RSocketResponder> requestResponder,
    std::shared_ptr<RSocketStats> stats,
    std::shared_ptr<RSocketNetworkStats> networkStats) {
  createRSocketStateMachine(std::move(requestResponder), std::move(stats), std::move(networkStats));
}

std::shared_ptr<RSocketStateMachine> RSocketSetup::createRSocketStateMachine(
    std::shared_ptr<RSocketResponder> requestResponder,
    std::shared_ptr<RSocketStats> stats,
    std::shared_ptr<RSocketNetworkStats> networkStats) {
  if(requestResponder) {
    requestResponder = std::make_shared<ScheduledRSocketResponder>(
        std::move(requestResponder), eventBase_);
  } else {
    // if the responder was not provided, we will create a default one
    requestResponder = std::make_shared<RSocketResponder>();
  }

  if (!stats) {
    stats = RSocketStats::noop();
  }

  auto rs = std::make_shared<RSocketStateMachine>(
      eventBase_,
      std::move(requestResponder),
      nullptr,
      ReactiveSocketMode::SERVER,
      std::move(stats),
      std::move(networkStats));

  connectionManager_.manageConnection(rs, eventBase_);
  rs->connectServer(std::move(frameTransport_), setupParams_);
  return rs;
}

void RSocketSetup::error(const RSocketError& error) {
  // TODO emit ERROR ... but how do I do that here?
  frameTransport_->closeWithError(std::runtime_error(error.what()));
  frameTransport_ = nullptr;
}

}

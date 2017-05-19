// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketConnectionHandler.h"
#include <atomic>

#include <folly/io/async/EventBase.h>
#include <folly/io/async/EventBaseManager.h>

#include "RSocketErrors.h"
#include "RSocketStats.h"
#include "src/statemachine/RSocketStateMachine.h"

namespace rsocket {

using namespace rsocket;
using namespace yarpl;

void RSocketConnectionHandler::setupNewSocket(
    std::shared_ptr<FrameTransport> frameTransport,
    SetupParameters setupPayload) {
  LOG(INFO) << "RSocketServer => received new setup payload";

  // FIXME(alexanderm): Handler should be tied to specific executor
  auto executor = folly::EventBaseManager::get()->getExistingEventBase();

  auto socketParams =
      RSocketParameters(setupPayload.resumable, setupPayload.protocolVersion);
  std::shared_ptr<ConnectionSetupRequest> setupRequest =
      std::make_shared<ConnectionSetupRequest>(std::move(setupPayload));
  std::shared_ptr<RSocketResponder> requestResponder;
  try {
    requestResponder = getRequestResponder(setupRequest);
  } catch (const RSocketError& e) {
    // TODO emit ERROR ... but how do I do that here?
    frameTransport->close(
        folly::exception_wrapper{std::current_exception(), e});
    return;
  }
  LOG(INFO) << "RSocketServer => received request handler";

  if (!requestResponder) {
    // if the responder was not provided, we will create a default one
    requestResponder = std::make_shared<RSocketResponder>();
  }

  auto rs = std::make_shared<RSocketStateMachine>(
      *executor,
      std::move(requestResponder),
      RSocketStats::noop(),
      nullptr,
      ReactiveSocketMode::SERVER);

  manageSocket(setupRequest, rs);

  // TODO ---> this code needs to be moved inside RSocketStateMachine

  // Connect last, after all state has been set up.
  rs->setResumable(socketParams.resumable);

  if (socketParams.protocolVersion != ProtocolVersion::Unknown) {
    rs->setFrameSerializer(
        FrameSerializer::createFrameSerializer(socketParams.protocolVersion));
  }

  rs->connect(std::move(frameTransport), true, socketParams.protocolVersion);

  // TODO <---- up to here
  // TODO and then a simple API such as:
  // TODO rs->connectServer(frameTransport, params)
}

bool RSocketConnectionHandler::resumeSocket(
    std::shared_ptr<FrameTransport> frameTransport,
    ResumeParameters) {
  return false;
}

void RSocketConnectionHandler::connectionError(
    std::shared_ptr<FrameTransport>,
    folly::exception_wrapper ex) {
  LOG(WARNING) << "Connection failed before first frame: " << ex.what();
}

} // namespace rsocket

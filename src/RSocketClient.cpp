// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketClient.h"
#include "RSocketRequester.h"
#include "src/internal/FollyKeepaliveTimer.h"
#include "src/temporary_home/NullRequestHandler.h"
#include "src/temporary_home/Stats.h"

using namespace reactivesocket;
using namespace folly;

namespace rsocket {

RSocketClient::RSocketClient(std::unique_ptr<ConnectionFactory> connection)
    : lazyConnection_(std::move(connection)) {
  LOG(INFO) << "RSocketClient => created";
}

Future<std::shared_ptr<RSocketRequester>> RSocketClient::connect() {
  LOG(INFO) << "RSocketClient => start connection with Future";

  auto promise = std::make_shared<Promise<std::shared_ptr<RSocketRequester>>>();

  lazyConnection_->connect([this, promise](
      std::unique_ptr<DuplexConnection> framedConnection,
      EventBase& eventBase) {
    LOG(INFO) << "RSocketClient => onConnect received DuplexConnection";

    auto rs = std::make_shared<RSocketStateMachine>(
        eventBase,
        // need to allow Responder being passed in optionally
        std::make_unique<NullRequestHandler>(),
        // need to allow stats being passed in
        Stats::noop(),
        // TODO need to optionally allow defining the keepalive timer
        std::make_unique<FollyKeepaliveTimer>(
            eventBase, std::chrono::milliseconds(5000)),
        ReactiveSocketMode::CLIENT);

    // TODO need to allow this being passed in
    auto setupPayload = ConnectionSetupPayload(
        "text/plain", "text/plain", Payload("meta", "data"));

    // TODO ---> this code needs to be moved inside RSocketStateMachine

    rs->setFrameSerializer(
        setupPayload.protocolVersion == ProtocolVersion::Unknown
            ? FrameSerializer::createCurrentVersion()
            : FrameSerializer::createFrameSerializer(
                  setupPayload.protocolVersion));

    rs->setResumable(setupPayload.resumable);

    if (setupPayload.protocolVersion != ProtocolVersion::Unknown) {
      CHECK_EQ(
          setupPayload.protocolVersion, rs->getSerializerProtocolVersion());
    }

    auto frameTransport =
        std::make_shared<FrameTransport>(std::move(framedConnection));
    rs->setUpFrame(std::move(frameTransport), std::move(setupPayload));

    // TODO <---- up to here
    // TODO and then a simple API such as:
    // TODO rs->connectAndSendSetup(frameTransport, params, setupPayload)

    auto rsocket = RSocketRequester::create(std::move(rs), eventBase);
    // store it so it lives as long as the RSocketClient
    rsockets_.push_back(rsocket);
    promise->setValue(rsocket);
  });

  return promise->getFuture();
}

RSocketClient::~RSocketClient() {
  LOG(INFO) << "RSocketClient => destroy";
}
}

// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketClient.h"
#include "RSocketRequester.h"
#include "RSocketResponder.h"
#include "RSocketStats.h"
#include "src/internal/FollyKeepaliveTimer.h"
#include "src/framing/FrameTransport.h"

using namespace rsocket;
using namespace folly;

namespace rsocket {

RSocketClient::RSocketClient(std::unique_ptr<ConnectionFactory> connectionFactory)
    : connectionFactory_(std::move(connectionFactory)) {
  LOG(INFO) << "RSocketClient => created";
}

Future<std::shared_ptr<RSocketRequester>> RSocketClient::connect() {
  LOG(INFO) << "RSocketClient => start connection with Future";

  auto promise = std::make_shared<Promise<std::shared_ptr<RSocketRequester>>>();
  auto future = promise->getFuture();

  connectionFactory_->connect([this, promise = std::move(promise)](
      std::unique_ptr<DuplexConnection> framedConnection,
      EventBase& eventBase) {
    LOG(INFO) << "RSocketClient => onConnect received DuplexConnection";

    auto rs = std::make_shared<RSocketStateMachine>(
        eventBase,
        // need to allow Responder being passed in optionally
        std::make_shared<RSocketResponder>(),
        // need to allow stats being passed in
        RSocketStats::noop(),
        // TODO need to optionally allow defining the keepalive timer
        std::make_unique<FollyKeepaliveTimer>(
            eventBase, std::chrono::milliseconds(5000)),
        ReactiveSocketMode::CLIENT);

    // TODO need to allow this being passed in
    auto setupPayload =
        SetupParameters("text/plain", "text/plain", Payload("meta", "data"));

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

  return future;
}

RSocketClient::~RSocketClient() {
  LOG(INFO) << "RSocketClient => destroy";
}
}

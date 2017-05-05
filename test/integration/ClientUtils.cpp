// Copyright 2004-present Facebook. All Rights Reserved.

#include "test/integration/ClientUtils.h"

namespace reactivesocket {
namespace tests {

// Utility function to create a FrameTransport.
std::shared_ptr<FrameTransport> getFrameTransport(
    folly::EventBase* eventBase,
    folly::AsyncSocket::ConnectCallback* connectCb,
    uint32_t port) {
  folly::SocketAddress addr;
  addr.setFromLocalPort(folly::to<uint16_t>(port));
  folly::AsyncSocket::UniquePtr socket(new folly::AsyncSocket(eventBase));
  socket->connect(connectCb, addr);
  LOG(INFO) << "Attempting connection to " << addr.describe();
  std::unique_ptr<DuplexConnection> connection =
      std::make_unique<TcpDuplexConnection>(
          std::move(socket), inlineExecutor(), Stats::noop());
  std::unique_ptr<DuplexConnection> framedConnection =
      std::make_unique<FramedDuplexConnection>(
          std::move(connection), *eventBase);
  return std::make_shared<FrameTransport>(std::move(framedConnection));
}

// Utility function to create a ReactiveSocket.
std::unique_ptr<ReactiveSocket> getRSocket(
    folly::EventBase* eventBase,
    std::shared_ptr<ResumeCache> resumeCache) {
  std::unique_ptr<ReactiveSocket> rsocket;
  std::unique_ptr<RequestHandler> requestHandler =
      std::make_unique<ClientRequestHandler>();
  rsocket = ReactiveSocket::disconnectedClient(
      *eventBase,
      std::move(requestHandler),
      std::move(resumeCache),
      Stats::noop(),
      std::make_unique<FollyKeepaliveTimer>(
          *eventBase, std::chrono::seconds(10)));
  rsocket->onConnected([]() { LOG(INFO) << "ClientSocket connected"; });
  rsocket->onDisconnected([](const folly::exception_wrapper& ex) {
    LOG(INFO) << "ClientSocket disconnected: " << ex.what();
  });
  rsocket->onClosed([](const folly::exception_wrapper& ex) {
    LOG(INFO) << "ClientSocket closed: " << ex.what();
  });
  return rsocket;
}

// Utility function to create a SetupPayload
ConnectionSetupPayload getSetupPayload(ResumeIdentificationToken token) {
  return ConnectionSetupPayload(
      "text/plain", "text/plain", Payload("meta", "data"), true, token);
}
}
}

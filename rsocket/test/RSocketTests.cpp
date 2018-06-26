// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rsocket/test/RSocketTests.h"

#include "rsocket/internal/WarmResumeManager.h"
#include "rsocket/test/test_utils/GenericRequestResponseHandler.h"
#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"

namespace rsocket {
namespace tests {
namespace client_server {

std::unique_ptr<TcpConnectionFactory> getConnFactory(
    folly::EventBase* eventBase,
    uint16_t port) {
  folly::SocketAddress address{"127.0.0.1", port};
  return std::make_unique<TcpConnectionFactory>(*eventBase, std::move(address));
}

std::unique_ptr<RSocketServer> makeServer(
    std::shared_ptr<rsocket::RSocketResponder> responder,
    std::shared_ptr<RSocketStats> stats) {
  TcpConnectionAcceptor::Options opts;
  opts.threads = 2;
  opts.address = folly::SocketAddress("0.0.0.0", 0);

  // RSocket server accepting on TCP.
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)),
      std::move(stats));

  rs->start([r = std::move(responder)](const SetupParameters&) { return r; });
  return rs;
}

std::unique_ptr<RSocketServer> makeResumableServer(
    std::shared_ptr<RSocketServiceHandler> serviceHandler) {
  TcpConnectionAcceptor::Options opts;
  opts.threads = 10;
  opts.backlog = 200;
  opts.address = folly::SocketAddress("0.0.0.0", 0);
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));
  rs->start(std::move(serviceHandler));
  return rs;
}

folly::Future<std::unique_ptr<RSocketClient>> makeClientAsync(
    folly::EventBase* eventBase,
    uint16_t port,
    folly::EventBase* stateMachineEvb,
    std::shared_ptr<RSocketStats> stats) {
  CHECK(eventBase);
  return RSocket::createConnectedClient(
      getConnFactory(eventBase, port),
      SetupParameters(),
      std::make_shared<RSocketResponder>(),
      kDefaultKeepaliveInterval,
      std::move(stats),
      std::shared_ptr<RSocketConnectionEvents>(),
      ResumeManager::makeEmpty(),
      std::shared_ptr<ColdResumeHandler>(),
      stateMachineEvb);
}

std::unique_ptr<RSocketClient> makeClient(
    folly::EventBase* eventBase,
    uint16_t port,
    folly::EventBase* stateMachineEvb,
    std::shared_ptr<RSocketStats> stats) {
  return makeClientAsync(eventBase, port, stateMachineEvb, std::move(stats))
      .get();
}

namespace {
struct DisconnectedResponder : public rsocket::RSocketResponder {
  DisconnectedResponder() {}

  std::shared_ptr<yarpl::single::Single<rsocket::Payload>>
  handleRequestResponse(rsocket::Payload, rsocket::StreamId) override {
    CHECK(false);
    return nullptr;
  }

  std::shared_ptr<yarpl::flowable::Flowable<rsocket::Payload>>
  handleRequestStream(rsocket::Payload, rsocket::StreamId) override {
    CHECK(false);
    return nullptr;
  }

  std::shared_ptr<yarpl::flowable::Flowable<rsocket::Payload>>
  handleRequestChannel(
      rsocket::Payload,
      std::shared_ptr<yarpl::flowable::Flowable<rsocket::Payload>>,
      rsocket::StreamId) override {
    CHECK(false);
    return nullptr;
  }

  void handleFireAndForget(rsocket::Payload, rsocket::StreamId) override {
    CHECK(false);
  }

  void handleMetadataPush(std::unique_ptr<folly::IOBuf>) override {
    CHECK(false);
  }

  ~DisconnectedResponder() {}
};
} // namespace

std::unique_ptr<RSocketClient> makeDisconnectedClient(
    folly::EventBase* eventBase) {
  auto server = makeServer(std::make_shared<DisconnectedResponder>());

  auto client = makeClient(eventBase, *server->listeningPort());
  client->disconnect().get();
  return client;
}

std::unique_ptr<RSocketClient> makeWarmResumableClient(
    folly::EventBase* eventBase,
    uint16_t port,
    std::shared_ptr<RSocketConnectionEvents> connectionEvents,
    folly::EventBase* stateMachineEvb) {
  CHECK(eventBase);
  SetupParameters setupParameters;
  setupParameters.resumable = true;
  return RSocket::createConnectedClient(
             getConnFactory(eventBase, port),
             std::move(setupParameters),
             std::make_shared<RSocketResponder>(),
             kDefaultKeepaliveInterval,
             RSocketStats::noop(),
             std::move(connectionEvents),
             std::make_shared<WarmResumeManager>(RSocketStats::noop()),
             std::shared_ptr<ColdResumeHandler>(),
             stateMachineEvb)
      .get();
}

std::unique_ptr<RSocketClient> makeColdResumableClient(
    folly::EventBase* eventBase,
    uint16_t port,
    ResumeIdentificationToken token,
    std::shared_ptr<ResumeManager> resumeManager,
    std::shared_ptr<ColdResumeHandler> coldResumeHandler,
    folly::EventBase* stateMachineEvb) {
  SetupParameters setupParameters;
  setupParameters.resumable = true;
  setupParameters.token = token;
  return RSocket::createConnectedClient(
             getConnFactory(eventBase, port),
             std::move(setupParameters),
             nullptr, // responder
             kDefaultKeepaliveInterval,
             nullptr, // stats
             nullptr, // connectionEvents
             resumeManager,
             coldResumeHandler,
             stateMachineEvb)
      .get();
}

} // namespace client_server
} // namespace tests
} // namespace rsocket

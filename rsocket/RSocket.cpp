// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/io/async/EventBaseManager.h>

#include "rsocket/RSocket.h"

namespace rsocket {

folly::Future<std::shared_ptr<RSocketClient>> RSocket::createConnectedClient(
    std::unique_ptr<ConnectionFactory> connectionFactory,
    SetupParameters setupParameters,
    std::shared_ptr<RSocketResponder> responder,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    std::shared_ptr<RSocketStats> stats,
    std::shared_ptr<RSocketNetworkStats> networkStats,
    std::shared_ptr<ResumeManager> resumeManager,
    std::shared_ptr<ColdResumeHandler> coldResumeHandler,
    OnRSocketResume) {
  auto c = std::shared_ptr<RSocketClient>(new RSocketClient(
      std::move(connectionFactory),
      std::move(setupParameters),
      std::move(responder),
      std::move(keepaliveTimer),
      std::move(stats),
      std::move(networkStats),
      std::move(resumeManager),
      std::move(coldResumeHandler)));

  return c->connect().then([c]() mutable { return c; });
}

folly::Future<std::shared_ptr<RSocketClient>> RSocket::createResumedClient(
    std::unique_ptr<ConnectionFactory> connectionFactory,
    SetupParameters setupParameters,
    std::shared_ptr<ResumeManager> resumeManager,
    std::shared_ptr<ColdResumeHandler> coldResumeHandler,
    OnRSocketResume,
    std::shared_ptr<RSocketResponder> responder,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    std::shared_ptr<RSocketStats> stats,
    std::shared_ptr<RSocketNetworkStats> networkStats) {
  auto c = std::shared_ptr<RSocketClient>(new RSocketClient(
      std::move(connectionFactory),
      std::move(setupParameters),
      std::move(responder),
      std::move(keepaliveTimer),
      std::move(stats),
      std::move(networkStats),
      std::move(resumeManager),
      std::move(coldResumeHandler)));

  return c->resume().then([c]() mutable { return c; });
}

std::unique_ptr<RSocketServer> RSocket::createServer(
    std::unique_ptr<ConnectionAcceptor> connectionAcceptor) {
  return std::make_unique<RSocketServer>(std::move(connectionAcceptor));
}
}

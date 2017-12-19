// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <utility>

#include "rsocket/RSocket.h"

#include "rsocket/transports/tcp/TcpConnectionFactory.h"
#include "yarpl/test_utils/utils.h"

namespace rsocket {
namespace tests {
namespace client_server {

class RSocketStatsFlowControl : public RSocketStats {
public:
  void frameWritten(FrameType frameType) {
    if (frameType == FrameType::REQUEST_N) {
      ++writeRequestN_;
    }
  }

  void frameRead(FrameType frameType) {
    if (frameType == FrameType::REQUEST_N) {
      ++readRequestN_;
    }
  }

public:
  int writeRequestN_{0};
  int readRequestN_{0};
};

std::unique_ptr<TcpConnectionFactory> getConnFactory(
    folly::EventBase* eventBase,
    uint16_t port);

std::unique_ptr<RSocketServer> makeServer(
    std::shared_ptr<rsocket::RSocketResponder> responder,
    std::shared_ptr<RSocketStats> stats = RSocketStats::noop());

std::unique_ptr<RSocketServer> makeResumableServer(
    std::shared_ptr<RSocketServiceHandler> serviceHandler);

std::unique_ptr<RSocketClient> makeClient(
    folly::EventBase* eventBase,
    uint16_t port,
    folly::EventBase* stateMachineEvb = nullptr,
    std::shared_ptr<RSocketStats> stats = RSocketStats::noop());

std::unique_ptr<RSocketClient> makeDisconnectedClient(
    folly::EventBase* eventBase);

folly::Future<std::unique_ptr<RSocketClient>> makeClientAsync(
    folly::EventBase* eventBase,
    uint16_t port,
    folly::EventBase* stateMachineEvb = nullptr,
    std::shared_ptr<RSocketStats> stats = RSocketStats::noop());

std::unique_ptr<RSocketClient> makeWarmResumableClient(
    folly::EventBase* eventBase,
    uint16_t port,
    std::shared_ptr<RSocketConnectionEvents> connectionEvents = nullptr,
    folly::EventBase* stateMachineEvb = nullptr);

std::unique_ptr<RSocketClient> makeColdResumableClient(
    folly::EventBase* eventBase,
    uint16_t port,
    ResumeIdentificationToken token,
    std::shared_ptr<ResumeManager> resumeManager,
    std::shared_ptr<ColdResumeHandler> resumeHandler,
    folly::EventBase* stateMachineEvb = nullptr);

} // namespace client_server
} // namespace tests
} // namespace rsocket

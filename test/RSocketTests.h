// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <utility>

#include "rsocket/RSocket.h"

#include "rsocket/transports/tcp/TcpConnectionFactory.h"

auto const default_baton_timeout = std::chrono::milliseconds(100);
#define CHECK_WAIT(baton) CHECK(baton.timed_wait(default_baton_timeout))

namespace rsocket {
namespace tests {
namespace client_server {

std::unique_ptr<TcpConnectionFactory> getConnFactory(
    folly::EventBase* eventBase,
    uint16_t port);

std::unique_ptr<RSocketServer> makeServer(
    std::shared_ptr<rsocket::RSocketResponder> responder);

std::unique_ptr<RSocketServer> makeResumableServer(
    std::shared_ptr<RSocketServiceHandler> serviceHandler);

std::unique_ptr<RSocketClient> makeClient(
    folly::EventBase* eventBase,
    uint16_t port);

std::unique_ptr<RSocketClient> makeDisconnectedClient(
    folly::EventBase* eventBase);

folly::Future<std::unique_ptr<RSocketClient>> makeClientAsync(
    folly::EventBase* eventBase,
    uint16_t port);

std::unique_ptr<RSocketClient> makeWarmResumableClient(
    folly::EventBase* eventBase,
    uint16_t port,
    std::shared_ptr<RSocketConnectionEvents> connectionEvents = nullptr);

std::unique_ptr<RSocketClient> makeColdResumableClient(
    folly::EventBase* eventBase,
    uint16_t port,
    ResumeIdentificationToken token,
    std::shared_ptr<ResumeManager> resumeManager,
    std::shared_ptr<ColdResumeHandler> resumeHandler);

} // namespace client_server
} // namespace tests
} // namespace rsocket

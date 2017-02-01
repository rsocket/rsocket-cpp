// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <list>
#include <memory>

#include "src/Common.h"
#include "src/ConnectionSetupPayload.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/Stats.h"

namespace folly {
class Executor;
}

namespace reactivesocket {
class ClientResumeStatusCallback;
class ConnectionAutomaton;
class DuplexConnection;
class FrameTransport;

class ReactiveSocket {
 public:
  virtual ~ReactiveSocket() = default;

  virtual std::shared_ptr<Subscriber<Payload>> requestChannel(
      std::shared_ptr<Subscriber<Payload>> responseSink) = 0;

  virtual void requestStream(
      Payload payload,
      const std::shared_ptr<Subscriber<Payload>> responseSink) = 0;

  virtual void requestSubscription(
      Payload payload,
      const std::shared_ptr<Subscriber<Payload>> responseSink) = 0;

  virtual void requestResponse(
      Payload payload,
      const std::shared_ptr<Subscriber<Payload>> responseSink) = 0;

  virtual void requestFireAndForget(Payload request) = 0;

  virtual void metadataPush(std::unique_ptr<folly::IOBuf> metadata) = 0;

  virtual void clientConnect(
      std::shared_ptr<FrameTransport> frameTransport,
      ConnectionSetupPayload setupPayload = ConnectionSetupPayload()) = 0;

  virtual void serverConnect(
      std::shared_ptr<FrameTransport> frameTransport,
      bool isResumable) = 0;

  virtual void close() = 0;
  virtual void disconnect() = 0;
  virtual std::shared_ptr<FrameTransport> detachFrameTransport() = 0;

  virtual void onConnected(std::function<void()> listener) = 0;
  virtual void onDisconnected(ErrorCallback listener) = 0;
  virtual void onClosed(ErrorCallback listener) = 0;

  virtual void tryClientResume(
      const ResumeIdentificationToken& token,
      std::shared_ptr<FrameTransport> frameTransport,
      std::unique_ptr<ClientResumeStatusCallback> resumeCallback) = 0;

  virtual bool tryResumeServer(
      std::shared_ptr<FrameTransport> frameTransport,
      ResumePosition position) = 0;

  virtual folly::Executor& executor() = 0;

  virtual DuplexConnection* duplexConnection() const = 0;
};
}

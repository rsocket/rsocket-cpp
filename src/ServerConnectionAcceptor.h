// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/Common.h"
#include "src/ConnectionSetupPayload.h"

#include <folly/Baton.h>
#include <folly/Synchronized.h>

#include <memory>
#include <mutex>
#include <unordered_set>

namespace folly {
class EventBase;
class Executor;
class exception_wrapper;
class IOBuf;
}

namespace reactivesocket {

class DuplexConnection;
class FrameSerializer;
class FrameTransport;
class Stats;
class OneFrameProcessor;

class ConnectionHandler {
 public:
  virtual ~ConnectionHandler() = default;

  /// Called when we've received a setup frame on the connection and are ready
  /// to make a new ReactiveSocket.
  /// frameTransport parameter needs to be assigned to a new instance of
  /// ReactiveSocket or closed otherwise it will be leaked (until the
  /// connection fails)
  virtual void setupNewSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ConnectionSetupPayload setupPayload) = 0;

  /// Called when we've received a resume frame on the connection and are ready
  /// to resume an existing ReactiveSocket.
  /// frameTransport parameter needs to be assigned to a new instance of
  /// ReactiveSocket or closed otherwise it will be leaked (until the
  /// connection fails)
  // Return true if you took ownership of the FrameTransport
  virtual bool resumeSocket(
      std::shared_ptr<FrameTransport>,
      ResumeParameters resumeParameters) = 0;

  virtual void connectionError(
      std::shared_ptr<FrameTransport>,
      folly::exception_wrapper ex) = 0;
};

// This class allows to store duplex connection and wait until the first frame
// is received. Then either onNewSocket or onResumeSocket is invoked.
class ServerConnectionAcceptor final {
 public:
  explicit ServerConnectionAcceptor(ProtocolVersion defaultProtocolVersion);
  ~ServerConnectionAcceptor();

  void accept(
      std::unique_ptr<DuplexConnection> connection,
      std::shared_ptr<ConnectionHandler> connectionHandler);

  /// Close all FrameTransports and wait until they all get handled.
  void stop();

 private:
  friend OneFrameProcessor;

  void processFrame(
      std::shared_ptr<ConnectionHandler> connectionHandler,
      std::shared_ptr<FrameTransport> transport,
      std::unique_ptr<folly::IOBuf> frame);

  void closeAndRemoveConnection(
      const std::shared_ptr<ConnectionHandler>& connectionHandler,
      std::shared_ptr<FrameTransport> transport,
      folly::exception_wrapper ex);

  void erase(std::shared_ptr<FrameTransport>);

  std::shared_ptr<FrameSerializer> getOrAutodetectFrameSerializer(
      const folly::IOBuf&);

 private:
  folly::Synchronized<
      std::unordered_set<std::shared_ptr<FrameTransport>>,
      std::mutex>
      connections_;

  folly::Optional<folly::Baton<>> shutdown_;

  std::shared_ptr<FrameSerializer> defaultFrameSerializer_;
};

} // reactivesocket

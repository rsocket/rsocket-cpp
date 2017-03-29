// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <unordered_set>
#include "src/Common.h"
#include "src/ConnectionSetupPayload.h"
#include "src/versions/FrameSerializer_v0_1.h"

namespace folly {
class EventBase;
class Executor;
class exception_wrapper;
class IOBuf;
}

namespace reactivesocket {

class DuplexConnection;
class FrameTransport;
class Stats;

// This class allows to store duplex connection and wait until the first frame
// is received. Then either onNewSocket or onResumeSocket is invoked.
class ServerConnectionAcceptor {
 public:
  virtual ~ServerConnectionAcceptor();

  /// Called when we've received a setup frame on the connection and are ready
  /// to make a new ReactiveSocket.
  /// frameTransport parameter needs to be assigned to a new instance of
  /// ReactiveSocket or closed otherwise it will be leaked (until the
  /// connection fails)
  virtual void setupNewSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ConnectionSetupPayload setupPayload,
      folly::Executor&) = 0;

  /// Called when we've received a resume frame on the connection and are ready
  /// to resume an existing ReactiveSocket.
  /// frameTransport parameter needs to be assigned to a new instance of
  /// ReactiveSocket or closed otherwise it will be leaked (until the
  /// connection fails)
  virtual void resumeSocket(
      std::shared_ptr<FrameTransport>,
      ResumeParameters,
      folly::Executor&) = 0;

  void acceptConnection(std::unique_ptr<DuplexConnection>, folly::Executor&);
  void removeConnection(const std::shared_ptr<FrameTransport>& transport);
  void processFrame(
      std::shared_ptr<FrameTransport> transport,
      std::unique_ptr<folly::IOBuf> frame,
      folly::Executor&);

 private:
  std::unordered_set<std::shared_ptr<FrameTransport>> connections_;
  FrameSerializerV0_1 frameSerializer_;
};

} // reactivesocket

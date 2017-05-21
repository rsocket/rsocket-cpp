// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <unordered_set>
#include "src/RSocketParameters.h"
#include "src/internal/Common.h"

namespace folly {
class EventBase;
class Executor;
class exception_wrapper;
class IOBuf;
}

namespace rsocket {

class DuplexConnection;
class FrameSerializer;
class FrameTransport;
class OneFrameProcessor;

// This class allows to store duplex connection and wait until the first frame
// is received. Then either onNewSocket or onResumeSocket is invoked.
class SetupResumeAcceptor final {
 public:
  using OnSetup = std::function<void(
      std::shared_ptr<FrameTransport> frameTransport, SetupParameters setupParams)>;
  using OnResume = std::function<void(
      std::shared_ptr<FrameTransport> frameTransport, ResumeParameters resumeParams)>;

  explicit SetupResumeAcceptor(ProtocolVersion defaultProtocolVersion);
  ~SetupResumeAcceptor();

  void accept(
      std::unique_ptr<DuplexConnection> connection,
      OnSetup onSetup,
      OnResume onResume);

 protected:
  friend OneFrameProcessor;

  void processFrame(
      std::shared_ptr<FrameTransport> transport,
      std::unique_ptr<folly::IOBuf> frame,
      OnSetup onSetup,
      OnResume onResume);

  void closeAndRemoveConnection(
      const std::shared_ptr<FrameTransport>& transport,
      folly::exception_wrapper ex);
  void removeConnection(const std::shared_ptr<FrameTransport>& transport);

 private:
  std::shared_ptr<FrameSerializer> getOrAutodetectFrameSerializer(
      const folly::IOBuf& firstFrame);

  std::unordered_set<std::shared_ptr<FrameTransport>> connections_;
  std::shared_ptr<FrameSerializer> defaultFrameSerializer_;
};

} // reactivesocket

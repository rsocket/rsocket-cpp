// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <unordered_set>

#include <folly/futures/Future.h>

#include "rsocket/RSocketParameters.h"
#include "rsocket/internal/Common.h"
#include "yarpl/Refcounted.h"

namespace folly {
class EventBase;
class Executor;
class IOBuf;
class exception_wrapper;
}

namespace rsocket {

class DuplexConnection;
class FrameSerializer;
class FrameTransport;

// This class allows to store duplex connection and wait until the first frame
// is received. Then either onSetup or onResume is invoked.
class SetupResumeAcceptor final {
 public:
  using OnSetup =
      std::function<void(yarpl::Reference<FrameTransport>, SetupParameters)>;
  using OnResume =
      std::function<bool(yarpl::Reference<FrameTransport>, ResumeParameters)>;

  explicit SetupResumeAcceptor(
      ProtocolVersion defaultProtocolVersion,
      folly::EventBase* eventBase);

  ~SetupResumeAcceptor();

  void accept(
      std::unique_ptr<DuplexConnection> connection,
      OnSetup onSetup,
      OnResume onResume);

  folly::Future<folly::Unit> close();

private:
  friend class OneFrameProcessor;

  void processFrame(
      yarpl::Reference<FrameTransport> transport,
      std::unique_ptr<folly::IOBuf> frame,
      OnSetup onSetup,
      OnResume onResume);

  void closeAndRemoveConnection(
      const yarpl::Reference<FrameTransport>& transport,
      folly::exception_wrapper ex);
  void removeConnection(const yarpl::Reference<FrameTransport>& transport);

  void closeAllConnections();

  std::shared_ptr<FrameSerializer> getOrAutodetectFrameSerializer(
      const folly::IOBuf& firstFrame);

  std::unordered_set<yarpl::Reference<FrameTransport>> connections_;
  bool closed_{false};

  std::shared_ptr<FrameSerializer> defaultFrameSerializer_;
  folly::EventBase* eventBase_;
};

} // reactivesocket

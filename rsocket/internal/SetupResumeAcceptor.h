// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <unordered_set>

#include <folly/Function.h>
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

/// Acceptor of DuplexConnections that lets us decide whether the connection is
/// trying to setup a new connection or resume an existing one.
///
/// An instance of this class must be tied to a specific thread, as the
/// SetupResumeAcceptor::accept() entry point is not thread-safe.
class SetupResumeAcceptor final {
 public:
  using OnSetup =
      folly::Function<void(yarpl::Reference<FrameTransport>, SetupParameters)>;
  using OnResume =
      folly::Function<void(yarpl::Reference<FrameTransport>, ResumeParameters)>;

  SetupResumeAcceptor(ProtocolVersion, folly::EventBase*);
  ~SetupResumeAcceptor();

  /// Wait for and process the first frame on a DuplexConnection, calling the
  /// appropriate callback when the frame is received.  Not thread-safe.
  void accept(std::unique_ptr<DuplexConnection>, OnSetup, OnResume);

  /// Close all open connections, and prevent new ones from being accepted.  Can
  /// be called from any thread.
  folly::Future<folly::Unit> close();

 private:
  friend class OneFrameProcessor;

  void processFrame(
      yarpl::Reference<FrameTransport>,
      std::unique_ptr<folly::IOBuf>,
      OnSetup,
      OnResume);

  /// Close and remove a FrameTransport from the set.
  void close(yarpl::Reference<FrameTransport>, folly::exception_wrapper);

  /// Remove a FrameTransport from the set.  Drop the attached OneFrameProcessor
  /// if it has one.
  void remove(const yarpl::Reference<FrameTransport>&);

  /// Close all open connections.
  void closeAll();

  /// Get the default FrameSerializer if one exists, otherwise try to autodetect
  /// the correct FrameSerializer from the given frame.
  std::shared_ptr<FrameSerializer> createSerializer(const folly::IOBuf&);

  std::unordered_set<yarpl::Reference<FrameTransport>> connections_;
  bool closed_{false};

  std::shared_ptr<FrameSerializer> defaultSerializer_;
  folly::EventBase* eventBase_;
};
}

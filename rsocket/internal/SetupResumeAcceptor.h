// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <unordered_set>

#include <folly/Function.h>
#include <folly/futures/Future.h>

#include "rsocket/DuplexConnection.h"
#include "rsocket/RSocketParameters.h"
#include "yarpl/Refcounted.h"

namespace folly {
class EventBase;
class Executor;
class IOBuf;
class exception_wrapper;
} // namespace folly

namespace rsocket {

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

  explicit SetupResumeAcceptor(folly::EventBase*);
  ~SetupResumeAcceptor();

  /// Wait for and process the first frame on a DuplexConnection, calling the
  /// appropriate callback when the frame is received.  Not thread-safe.
  void accept(std::unique_ptr<DuplexConnection>, OnSetup, OnResume);

  /// Close all open connections, and prevent new ones from being accepted.  Can
  /// be called from any thread, and also after the EventBase has been
  /// destroyed, provided we know the ID of the owner thread.
  folly::Future<folly::Unit> close();

 private:
  /// Subscriber that owns a connection, sets itself as that connection's input,
  /// and reads out a single frame before cancelling.
  class OneFrameSubscriber final
      : public yarpl::flowable::BaseSubscriber<std::unique_ptr<folly::IOBuf>> {
   public:
    OneFrameSubscriber(
        SetupResumeAcceptor&,
        std::unique_ptr<DuplexConnection>,
        SetupResumeAcceptor::OnSetup,
        SetupResumeAcceptor::OnResume);

    void setInput();

    /// Shut down the DuplexConnection, breaking the cycle between it and this
    /// subscriber.  Expects the DuplexConnection's destructor to call
    /// onComplete/onError on its input subscriber (this).
    void close();

    // Subscriber.
    void onSubscribeImpl() override;
    void onNextImpl(std::unique_ptr<folly::IOBuf>) override;
    void onCompleteImpl() override;
    void onErrorImpl(folly::exception_wrapper) override;
    void onTerminateImpl() override;

   private:
    SetupResumeAcceptor& acceptor_;
    std::unique_ptr<DuplexConnection> connection_;
    SetupResumeAcceptor::OnSetup onSetup_;
    SetupResumeAcceptor::OnResume onResume_;
  };

  void processFrame(
      std::unique_ptr<DuplexConnection>,
      std::unique_ptr<folly::IOBuf>,
      OnSetup,
      OnResume);

  /// Remove a OneFrameSubscriber from the set.
  void remove(const yarpl::Reference<OneFrameSubscriber>&);

  /// Close all open connections.
  void closeAll();

  /// Whether we're running in the thread that owns this object.  If the ctor
  /// specified an owner thread ID, then this will not access the EventBase
  /// pointer.
  ///
  /// Useful if the EventBase has been destroyed but we still want to do some
  /// work within the owner thread.
  bool inOwnerThread() const;

  std::unordered_set<yarpl::Reference<OneFrameSubscriber>> connections_;

  bool closed_{false};

  folly::EventBase* eventBase_;
};

} // namespace rsocket

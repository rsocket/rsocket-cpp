// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <reactive-streams/utilities/SmartPointers.h>

#include "src/DuplexConnection.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

/// An intra-thread implementation of DuplexConnection that synchronously passes
/// signals to the other connection it was connected to.
///
/// Accessing an input or an output of the InlineConnection before it has been
/// connected to another instance yields an undefined behaviour. Both connected
/// instances must outlive any of the inputs or outputs obtained from them.
///
/// This class is not thread-safe.
class InlineConnection : public DuplexConnection {
 public:
  InlineConnection() = default;

  // Noncopyable
  InlineConnection(const InlineConnection&) = delete;
  InlineConnection& operator=(const InlineConnection&) = delete;
  // Nonmovable
  InlineConnection(InlineConnection&&) = delete;
  InlineConnection& operator=(InlineConnection&&) = delete;
  ~InlineConnection();
  /// Connects this end of a DuplexConnection to another one.
  ///
  /// This method may be invoked at most once per lifetime of the object and
  /// implicitly connects the `other` to this instance. Must be invoked before
  /// accessing input or output of the connection.
  void connectTo(InlineConnection& other);

  void setInput(std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>
                    inputSink) override;

  std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>> getOutput()
      override;

 private:
  // This is to allow 2 instances of InlineConnection to point at each other
  // and write assertions about each other even if one InlineConnection
  // instance is destroyed, the other can validate state of the first one.
  // This is the case when client ReactiveSocket instance will be connected
  // to server ReactiveSocket over 2 instances of InlineConnection. When one
  // instance of ReactiveSocket is destroyed, the second one can still get
  // cleanly destroyed with all the validations in the terminating methods
  // (onComplete/onError/cancel) of the associated InlineConnection
  struct SharedState {
    std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>> inputSink_;
    /// @{
    /// Store pending terminal signal that would be sent to the input, if it was
    /// set at the time the signal was issued. Both fields being false indicate
    /// a
    /// situation where no terminal signal has been sent.
    bool inputSinkCompleted_{false};
    folly::exception_wrapper inputSinkError_;
    /// @}
    std::shared_ptr<Subscription> outputSubscription_;
    bool instanceTerminated_{false};
  };

  std::shared_ptr<SharedState> other_;
  std::shared_ptr<SharedState> this_{std::make_shared<SharedState>()};
  std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>> outputSink_;
};
}

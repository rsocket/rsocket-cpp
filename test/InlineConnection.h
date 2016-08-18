// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>

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
  // Noncopyable
  InlineConnection(const InlineConnection&) = delete;
  InlineConnection& operator=(const InlineConnection&) = delete;
  // Nonmovable
  InlineConnection(InlineConnection&&) = delete;
  InlineConnection& operator=(InlineConnection&&) = delete;

  InlineConnection();

  ~InlineConnection() override;

  /// Connects this end of a DuplexConnection to another one.
  ///
  /// This method may be invoked at most once per lifetime of the object and
  /// implicitly connects the `other` to this instance. Must be invoked before
  /// accessing input or output of the connection.
  void connectTo(InlineConnection& other, bool expectSetupFrame);

  void setInput(Subscriber<Payload>& inputSink) override;

  Subscriber<Payload>& getOutput() override;

 private:
  InlineConnection* other_;
  Subscriber<Payload>* inputSink_;
  /// @{
  /// Store pending terminal signal that would be sent to the input, if it was
  /// set at the time the signal was issued. Both fields being false indicate a
  /// situation where no terminal signal has been sent.
  bool inputSinkCompleted_;
  folly::exception_wrapper inputSinkError_;
  /// @}
  Subscription* outputSubscription_;
  bool expectSetupFrame_{false};
};
}

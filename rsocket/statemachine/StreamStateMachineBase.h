// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

#include <folly/ExceptionWrapper.h>

#include "rsocket/internal/Common.h"
#include "yarpl/Refcounted.h"

namespace folly {
class IOBuf;
}

namespace rsocket {

class StreamsWriter;
struct Payload;

/// A common base class of all state machines.
///
/// The instances might be destroyed on a different thread than they were
/// created.
class StreamStateMachineBase : public virtual yarpl::Refcounted {
 public:
  StreamStateMachineBase(
      std::shared_ptr<StreamsWriter> writer,
      StreamId streamId)
      : writer_{std::move(writer)}, streamId_(streamId) {}
  virtual ~StreamStateMachineBase() = default;

  virtual void handlePayload(Payload&& payload, bool complete, bool flagsNext);
  virtual void handleRequestN(uint32_t n);
  virtual void handleError(folly::exception_wrapper errorPayload);
  virtual void handleCancel();

  virtual size_t getConsumerAllowance() const;

  /// Indicates a terminal signal from the connection.
  ///
  /// This signal corresponds to Subscriber::{onComplete,onError} and
  /// Subscription::cancel.
  /// Per ReactiveStreams specification:
  /// 1. no other signal can be delivered during or after this one,
  /// 2. "unsubscribe handshake" guarantees that the signal will be delivered
  ///   exactly once, even if the state machine initiated stream closure,
  /// 3. per "unsubscribe handshake", the state machine must deliver
  /// corresponding
  ///   terminal signal to the connection.
  virtual void endStream(StreamCompletionSignal signal);

 protected:
  bool isTerminated() const {
    return isTerminated_;
  }

  void newStream(
      StreamType streamType,
      uint32_t initialRequestN,
      Payload payload,
      bool completed = false);
  void writePayload(Payload&& payload, bool complete);
  void writeRequestN(uint32_t n);
  void applicationError(std::string errorPayload);
  void errorStream(std::string errorPayload);
  void cancelStream();
  void completeStream();
  void closeStream(StreamCompletionSignal signal);

  /// A partially-owning pointer to the connection, the stream runs on.
  /// It is declared as const to allow only ctor to initialize it for thread
  /// safety of the dtor.
  const std::shared_ptr<StreamsWriter> writer_;
  const StreamId streamId_;
  // TODO: remove and nulify the writer_ instead
  bool isTerminated_{false};
};
}

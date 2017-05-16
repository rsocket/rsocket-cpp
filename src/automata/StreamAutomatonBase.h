// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <functional>
#include <iosfwd>
#include <memory>
#include "src/Common.h"
#include <folly/ExceptionWrapper.h>
#include <experimental/yarpl/include/yarpl/Refcounted.h>

namespace folly {
class IOBuf;
}

namespace reactivesocket {

class StreamsWriter;
class RequestHandler;
struct Payload;

///
/// A common base class of all automatons.
///
/// The instances might be destroyed on a different thread than they were
/// created.
class StreamAutomatonBase : public virtual yarpl::Refcounted {
 public:
  /// A dependent type which encapsulates all parameters needed to initialise
  /// any of the classes and the final automata. Must be the only argument to
  /// the
  /// constructor of any class and must be passed by const& to class's Base.
  struct Parameters {
    Parameters(std::shared_ptr<StreamsWriter> _writer, StreamId _streamId)
        : writer(std::move(_writer)), streamId(_streamId) {}

    std::shared_ptr<StreamsWriter> writer;
    StreamId streamId{0};
  };

  explicit StreamAutomatonBase(Parameters params)
      : writer_(std::move(params.writer)), streamId_(params.streamId) {}
  virtual ~StreamAutomatonBase() = default;

  virtual void handlePayload(Payload&& payload, bool complete, bool flagsNext);
  virtual void handleRequestN(uint32_t n);
  virtual void handleError(folly::exception_wrapper errorPayload);
  virtual void handleCancel();

  /// Indicates a terminal signal from the connection.
  ///
  /// This signal corresponds to Subscriber::{onComplete,onError} and
  /// Subscription::cancel.
  /// Per ReactiveStreams specification:
  /// 1. no other signal can be delivered during or after this one,
  /// 2. "unsubscribe handshake" guarantees that the signal will be delivered
  ///   exactly once, even if the automaton initiated stream closure,
  /// 3. per "unsubscribe handshake", the automaton must deliver corresponding
  ///   terminal signal to the connection.
  virtual void endStream(StreamCompletionSignal signal);
  /// @}

  virtual void pauseStream(RequestHandler& requestHandler) = 0;
  virtual void resumeStream(RequestHandler& requestHandler) = 0;

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

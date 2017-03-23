// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <functional>
#include <iosfwd>
#include <memory>
#include "src/Common.h"

namespace folly {
class IOBuf;
}

namespace reactivesocket {

class StreamsWriter;
class Frame_CANCEL;
class Frame_ERROR;
class Frame_REQUEST_CHANNEL;
class Frame_REQUEST_N;
class Frame_REQUEST_RESPONSE;
class Frame_REQUEST_STREAM;
class Frame_PAYLOAD;
class RequestHandler;
struct Payload;

///
/// A common base class of all automatons.
///
/// The instances might be destroyed on a different thread than they were
/// created.
class StreamAutomatonBase {
 public:
  /// A dependent type which encapsulates all parameters needed to initialise
  /// any of the mixins and the final automata. Must be the only argument to the
  /// constructor of any mixin and must be passed by const& to mixin's Base.
  struct Parameters {
    Parameters() = default;
    Parameters(std::shared_ptr<StreamsWriter> _writer, StreamId _streamId)
        : writer(std::move(_writer)), streamId(_streamId) {}

    std::shared_ptr<StreamsWriter> writer;
    StreamId streamId{0};
  };

  explicit StreamAutomatonBase(Parameters params)
      : writer_(std::move(params.writer)), streamId_(params.streamId) {}
  virtual ~StreamAutomatonBase() = default;

  /// @{
  /// A contract exposed to the connection, modelled after Subscriber and
  /// Subscription contracts while omitting flow control related signals.
  ///
  /// By omitting flow control between stream and channel automatons we greatly
  /// simplify implementation and move buffering into the connection automaton.

  /// A signal carrying serialized frame on the stream.
  ///
  /// This signal corresponds to Subscriber::onNext.
  void onNextFrame(std::unique_ptr<folly::IOBuf> frame);

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

  virtual void onNextFrame(Frame_REQUEST_STREAM&&);
  virtual void onNextFrame(Frame_REQUEST_CHANNEL&&);
  virtual void onNextFrame(Frame_REQUEST_RESPONSE&&);
  virtual void onNextFrame(Frame_REQUEST_N&&);
  virtual void onNextFrame(Frame_CANCEL&&);
  virtual void onNextFrame(Frame_PAYLOAD&&);
  virtual void onNextFrame(Frame_ERROR&&);

  void newStream(
      StreamType streamType,
      uint32_t initialRequestN,
      Payload payload,
      bool TEMP_completed = false);
  void writePayload(Payload&& payload, bool complete);
  void writeRequestN(uint32_t n);
  void applicationError(std::string errorPayload);
  void errorStream(std::string errorPayload);
  void cancelStream();
  void completeStream();
  void closeStream(StreamCompletionSignal signal);

 private:
  template <typename Frame>
  void deserializeAndDispatch(std::unique_ptr<folly::IOBuf> payload);

  void onBadFrame();
  void onUnknownFrame();

  /// A partially-owning pointer to the connection, the stream runs on.
  /// It is declared as const to allow only ctor to initialize it for thread
  /// safety of the dtor.
  const std::shared_ptr<StreamsWriter> writer_;
  const StreamId streamId_;
  // TODO: remove and nulify the writer_ instead
  bool isTerminated_{false};
};
}

// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <deque>
#include <memory>
#include <unordered_map>

#include "reactive-streams-cpp/utilities/AllowanceSemaphore.h"
#include "reactive-streams-cpp/utilities/SmartPointers.h"
#include "reactivesocket-cpp/src/Payload.h"
#include "reactivesocket-cpp/src/ReactiveStreamsCompat.h"

namespace lithium {
namespace reactivesocket {

class AbstractStreamAutomaton;
class DuplexConnection;
enum class FrameType : uint16_t;
enum class StreamCompletionSignal;
using StreamId = uint32_t;

/// Creates, registers and spins up responder for provided new stream ID and
/// serialised frame.
///
/// It is a responsibility of this startegy to register the responder with the
/// connection automaton and provide it with the initial frame if needed.
/// Returns true if the responder has been created successfully, false if the
/// frame cannot start a new stream, in which case the frame (passed by a
/// mutable referece) must not be modified.
using StreamAutomatonFactory = std::function<bool(StreamId, Payload&)>;

/// Handles connection-level frames and (de)multiplexes streams.
///
/// Instances of this class should be accessed and managed via shared_ptr,
/// instead of the pattern reflected in MemoryMixin and IntrusiveDeleter.
/// The reason why such a simple memory management story is possible lies in the
/// fact that there is no request(n)-based flow control between stream
/// automata and ConnectionAutomaton.
class ConnectionAutomaton :
    /// Registered as an input in the DuplexConnection.
    public Subscriber<Payload>,
    /// Receives signals about connection writability.
    public Subscription {
 public:
  ConnectionAutomaton(
      std::unique_ptr<DuplexConnection> connection,
      // TODO(stupaq): for testing only, can devirtualise if necessary
      StreamAutomatonFactory factory);

  /// Kicks off connection procedure.
  ///
  /// May result, depending on the implementation of the DuplexConnection, in
  /// processing of one or more frames.
  void connect();

  /// Terminates underlying connection.
  ///
  /// This may synchronously deliver terminal signals to all
  /// AbstractStreamAutomaton attached to this ConnectionAutomaton.
  void disconnect();

  ~ConnectionAutomaton();

  /// @{
  /// A contract exposed to AbstractStreamAutomaton, modelled after Subscriber
  /// and Subscription contracts, while omitting flow control related signals.

  /// Adds a stream automaton to the connection.
  ///
  /// This signal corresponds to Subscriber::onSubscribe.
  ///
  /// No frames will be issued as a result of this call. Stream automaton
  /// must take care of writing appropriate frames to the connection, using
  /// ::writeFrame after calling this method.
  void addStream(StreamId streamId, AbstractStreamAutomaton& automaton);

  /// Enqueues provided frame to be written to the underlying connection.
  /// Enqueuing a terminal frame does not end the stream.
  ///
  /// This signal corresponds to Subscriber::onNext.
  template <typename Frame>
  void onNextFrame(Frame& frame);
  void onNextFrame(Payload frame);

  /// Indicates that the stream should be removed from the connection.
  ///
  /// No frames will be issued as a result of this call. Stream automaton
  /// must take care of writing appropriate frames to the connection, using
  /// ::writeFrame, prior to calling this method.
  ///
  /// This signal corresponds to Subscriber::{onComplete,onError} and
  /// Subscription::cancel.
  /// Per ReactiveStreams specification:
  /// 1. no other signal can be delivered during or after this one,
  /// 2. "unsubscribe handshake" guarantees that the signal will be delivered
  ///   at least once, even if the automaton initiated stream closure,
  /// 3. per "unsubscribe handshake", the automaton must deliver corresponding
  ///   terminal signal to the connection.
  ///
  /// Additionally, in order to simplify implementation of stream automaton:
  /// 4. the signal bound with a particular StreamId is idempotent and may be
  ///   delivered multiple times as long as the caller holds shared_ptr to
  ///   ConnectionAutomaton.
  void endStream(StreamId streamId, StreamCompletionSignal signal);
  /// @}

 private:
  /// Performs the same actions as ::endStream without propagating closure
  /// signal to the underlying connection.
  ///
  /// The call is idempotent and returns false iff a stream has not been found.
  bool endStreamInternal(StreamId streamId, StreamCompletionSignal signal);

  /// @{
  void onSubscribe(Subscription&) override;

  void onNext(Payload) override;

  void onComplete() override;

  void onError(folly::exception_wrapper) override;

  void onTerminal(folly::exception_wrapper ex);
  /// @}

  /// @{
  void request(size_t) override;

  void cancel() override;
  /// @}

  /// @{
  /// State management at the connection level.
  void handleUnknownStream(StreamId streamId, Payload frame);
  /// @}

  std::unique_ptr<DuplexConnection> connection_;
  StreamAutomatonFactory factory_;
  // TODO(stupaq): looks like a bug that I have to qualify this
  reactivestreams::SubscriberPtr<reactivesocket::Subscriber<Payload>>
      connectionOutput_;
  reactivestreams::SubscriptionPtr<Subscription> connectionInputSub_;

  std::unordered_map<StreamId, AbstractStreamAutomaton*> streams_;
  reactivestreams::AllowanceSemaphore writeAllowance_;
  std::deque<Payload> pendingWrites_; // TODO(stupaq): two vectors?
};
}
}

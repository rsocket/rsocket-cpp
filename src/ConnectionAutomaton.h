// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <deque>
#include <memory>
#include <unordered_map>

#include <reactive-streams/utilities/AllowanceSemaphore.h>
#include <reactive-streams/utilities/SmartPointers.h>
#include "src/Payload.h"
#include "src/ReactiveSocket.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/ResumeCache.h"
#include "src/ResumeTracker.h"
#include "src/Stats.h"

namespace reactivesocket {

class AbstractStreamAutomaton;
class DuplexConnection;
enum class FrameType : uint16_t;
enum class StreamCompletionSignal;
using StreamId = uint32_t;

/// Creates, registers and spins up responder for provided new stream ID and
/// serialised frame.
///
/// It is a responsibility of this strategy to register the responder with the
/// connection automaton and provide it with the initial frame if needed.
/// Returns true if the responder has been created successfully, false if the
/// frame cannot start a new stream, in which case the frame (passed by a
/// mutable referece) must not be modified.
using StreamAutomatonFactory =
    std::function<bool(StreamId, std::unique_ptr<folly::IOBuf>)>;

using ResumeListener = std::function<
    bool(const ResumeIdentificationToken& token, ResumePosition position)>;

using ConnectionCloseListener = std::function<void()>;

/// Handles connection-level frames and (de)multiplexes streams.
///
/// Instances of this class should be accessed and managed via shared_ptr,
/// instead of the pattern reflected in MemoryMixin and IntrusiveDeleter.
/// The reason why such a simple memory management story is possible lies in the
/// fact that there is no request(n)-based flow control between stream
/// automata and ConnectionAutomaton.
class ConnectionAutomaton :
    /// Registered as an input in the DuplexConnection.
    public Subscriber<std::unique_ptr<folly::IOBuf>>,
    /// Receives signals about connection writability.
    public Subscription {
 public:
  ConnectionAutomaton(
      std::unique_ptr<DuplexConnection> connection,
      // TODO(stupaq): for testing only, can devirtualise if necessary
      StreamAutomatonFactory factory,
      ResumeListener resumeListener,
      Stats& stats,
      bool client);

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

  /// Terminate underlying connection and connect new connection
  void reconnect(std::unique_ptr<DuplexConnection> newConnection);

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
  void outputFrameOrEnqueue(std::unique_ptr<folly::IOBuf> frame);

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

  /// Copy the streams and resumption information from a previous
  /// ConnectionAutomaton
  void resumeFromAutomaton(ConnectionAutomaton& oldAutomaton);
  /// @}

  void sendKeepalive();
  void sendResume(const ResumeIdentificationToken& token);

  bool isPositionAvailable(ResumePosition position);
  ResumePosition positionDifference(ResumePosition position);

  void onClose(ConnectionCloseListener listener);

 private:
  /// Performs the same actions as ::endStream without propagating closure
  /// signal to the underlying connection.
  ///
  /// The call is idempotent and returns false iff a stream has not been found.
  bool endStreamInternal(StreamId streamId, StreamCompletionSignal signal);

  /// @{
  void onSubscribe(Subscription&) override;

  void onNext(std::unique_ptr<folly::IOBuf>) override;

  void onComplete() override;

  void onError(folly::exception_wrapper) override;

  void onTerminal(folly::exception_wrapper ex);

  void onConnectionFrame(std::unique_ptr<folly::IOBuf>);
  /// @}

  /// @{
  void request(size_t) override;

  void cancel() override;
  /// @}

  /// @{
  /// State management at the connection level.
  void handleUnknownStream(
      StreamId streamId,
      std::unique_ptr<folly::IOBuf> frame);
  /// @}

  void drainOutputFramesQueue();
  void outputFrame(std::unique_ptr<folly::IOBuf>);

  std::unique_ptr<DuplexConnection> connection_;
  StreamAutomatonFactory factory_;
  // TODO(stupaq): looks like a bug that I have to qualify this
  reactivestreams::SubscriberPtr<
      reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
      connectionOutput_;
  reactivestreams::SubscriptionPtr<Subscription> connectionInputSub_;

  std::unordered_map<StreamId, AbstractStreamAutomaton*> streams_;
  reactivestreams::AllowanceSemaphore writeAllowance_;
  std::deque<std::unique_ptr<folly::IOBuf>>
      pendingWrites_; // TODO(stupaq): two vectors?
  Stats& stats_;
  bool isServer_;
  bool isResumable_;
  std::vector<ConnectionCloseListener> closeListeners_;
  std::unique_ptr<ResumeTracker> resumeTracker_;
  std::unique_ptr<ResumeCache> resumeCache_;
  ResumeListener resumeListener_;
};
}

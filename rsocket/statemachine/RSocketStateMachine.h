// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <list>
#include <memory>

#include "rsocket/ColdResumeHandler.h"
#include "rsocket/DuplexConnection.h"
#include "rsocket/Payload.h"
#include "rsocket/RSocketParameters.h"
#include "rsocket/ResumeManager.h"
#include "rsocket/framing/FrameProcessor.h"
#include "rsocket/internal/Common.h"
#include "rsocket/internal/KeepaliveTimer.h"
#include "rsocket/statemachine/StreamState.h"
#include "rsocket/statemachine/StreamsFactory.h"
#include "rsocket/statemachine/StreamsWriter.h"

namespace rsocket {

class ClientResumeStatusCallback;
class ConnectionSet;
class DuplexConnection;
class FrameSerializer;
class FrameTransport;
class Frame_ERROR;
class KeepaliveTimer;
class RSocketConnectionEvents;
class RSocketParameters;
class RSocketResponder;
class RSocketStateMachine;
class RSocketStats;
class ResumeManager;
class StreamState;
class StreamStateMachineBase;

class FrameSink {
 public:
  virtual ~FrameSink() = default;

  /// Terminates underlying connection sending the error frame
  /// on the connection.
  ///
  /// This may synchronously deliver terminal signals to all
  /// StreamAutomatonBase attached to this ConnectionAutomaton.
  virtual void disconnectOrCloseWithError(Frame_ERROR&& error) = 0;

  virtual void sendKeepalive(
      std::unique_ptr<folly::IOBuf> data = folly::IOBuf::create(0)) = 0;
};

/// Handles connection-level frames and (de)multiplexes streams.
///
/// Instances of this class should be accessed and managed via shared_ptr,
/// instead of the pattern reflected in MemoryMixin and IntrusiveDeleter.
/// The reason why such a simple memory management story is possible lies in the
/// fact that there is no request(n)-based flow control between stream
/// automata and ConnectionAutomaton.
class RSocketStateMachine final
    : public FrameSink,
      public FrameProcessor,
      public StreamsWriter,
      public std::enable_shared_from_this<RSocketStateMachine> {
 public:
  RSocketStateMachine(
      std::shared_ptr<RSocketResponder> requestResponder,
      std::unique_ptr<KeepaliveTimer> keepaliveTimer,
      RSocketMode mode,
      std::shared_ptr<RSocketStats> stats,
      std::shared_ptr<RSocketConnectionEvents> connectionEvents,
      std::shared_ptr<ResumeManager> resumeManager,
      std::shared_ptr<ColdResumeHandler> coldResumeHandler);

  ~RSocketStateMachine();

  /// Create a new connection as a server.
  void connectServer(yarpl::Reference<FrameTransport>, const SetupParameters&);

  /// Resume a connection as a server.
  bool resumeServer(yarpl::Reference<FrameTransport>, const ResumeParameters&);

  /// Connect as a client.  Sends a SETUP frame.
  void connectClient(yarpl::Reference<FrameTransport>, SetupParameters);

  /// Resume a connection as a client.  Sends a RESUME frame.
  void resumeClient(
      ResumeIdentificationToken,
      yarpl::Reference<FrameTransport>,
      std::unique_ptr<ClientResumeStatusCallback>,
      ProtocolVersion);

  /// Disconnect the state machine's connection.  Existing streams will stay
  /// intact.
  void disconnect(folly::exception_wrapper);

  /// Whether the connection has been disconnected or closed.
  bool isDisconnected() const;

  /// Send an ERROR frame, and close the connection and all of its streams.
  void closeWithError(Frame_ERROR&&);

  /// Disconnect the connection if it is resumable, otherwise send an ERROR
  /// frame and close the connection and all of its streams.
  void disconnectOrCloseWithError(Frame_ERROR&&) override;

  /// Close the connection and all of its streams.
  void close(folly::exception_wrapper, StreamCompletionSignal);

  /// A contract exposed to StreamAutomatonBase, modelled after Subscriber
  /// and Subscription contracts, while omitting flow control related signals.

  /// Adds a stream stateMachine to the connection.
  ///
  /// This signal corresponds to Subscriber::onSubscribe.
  ///
  /// No frames will be issued as a result of this call. Stream stateMachine
  /// must take care of writing appropriate frames to the connection, using
  /// ::writeFrame after calling this method.
  void addStream(StreamId, yarpl::Reference<StreamStateMachineBase>);

  /// Indicates that the stream should be removed from the connection.
  ///
  /// No frames will be issued as a result of this call. Stream stateMachine
  /// must take care of writing appropriate frames to the connection, using
  /// ::writeFrame, prior to calling this method.
  ///
  /// This signal corresponds to Subscriber::{onComplete,onError} and
  /// Subscription::cancel.
  /// Per ReactiveStreams specification:
  /// 1. no other signal can be delivered during or after this one,
  /// 2. "unsubscribe handshake" guarantees that the signal will be delivered
  ///   at least once, even if the stateMachine initiated stream closure,
  /// 3. per "unsubscribe handshake", the stateMachine must deliver
  /// corresponding
  ///   terminal signal to the connection.
  ///
  /// Additionally, in order to simplify implementation of stream stateMachine:
  /// 4. the signal bound with a particular StreamId is idempotent and may be
  ///   delivered multiple times as long as the caller holds shared_ptr to
  ///   ConnectionAutomaton.
  void endStream(StreamId, StreamCompletionSignal);

  /// Send a REQUEST_FNF frame.
  void fireAndForget(Payload);

  /// Send a METADATA_PUSH frame.
  void metadataPush(std::unique_ptr<folly::IOBuf>);

  /// Send a KEEPALIVE frame, with the RESPOND flag set.
  void sendKeepalive(std::unique_ptr<folly::IOBuf>) override;

  /// Register the connection set that's holding this state machine.
  void registerSet(std::shared_ptr<ConnectionSet>);

  StreamsFactory& streamsFactory() {
    return streamsFactory_;
  }

  DuplexConnection* getConnection();

 private:
  void connect(yarpl::Reference<FrameTransport>, ProtocolVersion);

  /// Terminate underlying connection and connect new connection
  void reconnect(
      yarpl::Reference<FrameTransport>,
      std::unique_ptr<ClientResumeStatusCallback>);

  void setResumable(bool);

  bool resumeFromPositionOrClose(
      ResumePosition serverPosition,
      ResumePosition clientPosition);

  bool isPositionAvailable(ResumePosition) const;

  /// Whether the connection has been closed.
  bool isClosed() const;

  uint32_t getKeepaliveTime() const;

  void setFrameSerializer(std::unique_ptr<FrameSerializer>);

  void sendPendingFrames();

  /// Send a frame to the output.  Will buffer the frame if the state machine is
  /// disconnected or in the process of resuming.
  void outputFrameOrEnqueue(std::unique_ptr<folly::IOBuf>);

  template <typename T>
  void outputFrameOrEnqueue(T&& frame) {
    VLOG(3) << mode_ << " Out: " << frame;
    outputFrameOrEnqueue(
        frameSerializer_->serializeOut(std::forward<T>(frame)));
  }

  template <typename TFrame>
  bool deserializeFrameOrError(
      TFrame& frame,
      std::unique_ptr<folly::IOBuf> buf) {
    if (frameSerializer_->deserializeFrom(frame, std::move(buf))) {
      return true;
    }
    closeWithError(Frame_ERROR::connectionError("Invalid frame"));
    return false;
  }

  template <typename TFrame>
  bool deserializeFrameOrError(
      bool resumable,
      TFrame& frame,
      std::unique_ptr<folly::IOBuf> buf) {
    if (frameSerializer_->deserializeFrom(frame, std::move(buf), resumable)) {
      return true;
    }
    closeWithError(Frame_ERROR::connectionError("Invalid frame"));
    return false;
  }

  /// Performs the same actions as ::endStream without propagating closure
  /// signal to the underlying connection.
  ///
  /// The call is idempotent and returns false iff a stream has not been found.
  bool endStreamInternal(StreamId streamId, StreamCompletionSignal signal);

  // FrameProcessor.
  void processFrame(std::unique_ptr<folly::IOBuf>) override;
  void onTerminal(folly::exception_wrapper) override;

  void handleConnectionFrame(FrameType, std::unique_ptr<folly::IOBuf>);
  void handleStreamFrame(StreamId, FrameType, std::unique_ptr<folly::IOBuf>);
  void handleUnknownStream(StreamId, FrameType, std::unique_ptr<folly::IOBuf>);

  void closeStreams(StreamCompletionSignal);
  void closeFrameTransport(folly::exception_wrapper);

  void sendKeepalive(FrameFlags, std::unique_ptr<folly::IOBuf>);

  void resumeFromPosition(ResumePosition);
  void outputFrame(std::unique_ptr<folly::IOBuf>);

  void writeNewStream(
      StreamId streamId,
      StreamType streamType,
      uint32_t initialRequestN,
      Payload payload,
      bool completed) override;
  void writeRequestN(Frame_REQUEST_N&&) override;
  void writeCancel(Frame_CANCEL&&) override;

  void writePayload(Frame_PAYLOAD&&) override;
  void writeError(Frame_ERROR&&) override;

  void onStreamClosed(StreamId streamId, StreamCompletionSignal signal)
      override;

  bool ensureOrAutodetectFrameSerializer(const folly::IOBuf& firstFrame);

  size_t getConsumerAllowance(StreamId) const;

  /// Client/server mode this state machine is operating in.
  const RSocketMode mode_;

  /// Whether the connection was initialized as resumable.
  bool isResumable_{false};

  /// Whether the connection has closed.
  bool isClosed_{false};

  /// Whether a cold resume is currently in progress.
  bool coldResumeInProgress_{false};

  std::shared_ptr<RSocketStats> stats_;

  /// Per-stream frame buffer between the state machine and the FrameTransport.
  StreamState streamState_;

  // Manages all state needed for warm/cold resumption.
  std::shared_ptr<ResumeManager> resumeManager_;

  std::shared_ptr<RSocketResponder> requestResponder_;
  yarpl::Reference<FrameTransport> frameTransport_;
  std::unique_ptr<FrameSerializer> frameSerializer_;

  const std::unique_ptr<KeepaliveTimer> keepaliveTimer_;

  std::unique_ptr<ClientResumeStatusCallback> resumeCallback_;
  std::shared_ptr<ColdResumeHandler> coldResumeHandler_;

  StreamsFactory streamsFactory_;

  std::shared_ptr<RSocketConnectionEvents> connectionEvents_;

  /// Back reference to the set that's holding this state machine.
  std::weak_ptr<ConnectionSet> connectionSet_;
};
}

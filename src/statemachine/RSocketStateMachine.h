// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <list>
#include <memory>

#include "src/DuplexConnection.h"
#include "src/Payload.h"
#include "src/RSocketParameters.h"
#include "src/framing/FrameProcessor.h"
#include "src/internal/Common.h"
#include "src/statemachine/StreamsFactory.h"
#include "src/statemachine/StreamsWriter.h"

namespace rsocket {

class ClientResumeStatusCallback;
class DuplexConnection;
class Frame_ERROR;
class FrameSerializer;
class FrameTransport;
class KeepaliveTimer;
class ResumeCache;
class RSocketNetworkStats;
class RSocketParameters;
class RSocketResponder;
class RSocketStateMachine;
class RSocketStats;
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
      folly::Executor& executor,
      std::shared_ptr<RSocketResponder> requestResponder,
      std::unique_ptr<KeepaliveTimer> keepaliveTimer_,
      ReactiveSocketMode mode,
      std::shared_ptr<RSocketStats> stats,
      std::shared_ptr<RSocketNetworkStats> networkStats = std::shared_ptr<RSocketNetworkStats>());

  void closeWithError(Frame_ERROR&& error);
  void disconnectOrCloseWithError(Frame_ERROR&& error) override;

  /// Kicks off connection procedure.
  ///
  /// May result, depending on the implementation of the DuplexConnection, in
  /// processing of one or more frames.
  bool connectServer(
      yarpl::Reference<FrameTransport>,
      const SetupParameters& setupParams);

  /// Disconnects DuplexConnection from the stateMachine.
  /// Existing streams will stay intact.
  void disconnect(folly::exception_wrapper ex);

  /// Terminates underlying connection.
  ///
  /// This may synchronously deliver terminal signals to all
  /// StreamAutomatonBase attached to this ConnectionAutomaton.
  void close(folly::exception_wrapper, StreamCompletionSignal);

  /// Terminate underlying connection and connect new connection
  void reconnect(
      yarpl::Reference<FrameTransport>,
      std::unique_ptr<ClientResumeStatusCallback>);

  ~RSocketStateMachine();

  /// A contract exposed to StreamAutomatonBase, modelled after Subscriber
  /// and Subscription contracts, while omitting flow control related signals.

  /// Adds a stream stateMachine to the connection.
  ///
  /// This signal corresponds to Subscriber::onSubscribe.
  ///
  /// No frames will be issued as a result of this call. Stream stateMachine
  /// must take care of writing appropriate frames to the connection, using
  /// ::writeFrame after calling this method.
  void addStream(
      StreamId streamId,
      yarpl::Reference<StreamStateMachineBase> stateMachine);

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
  void endStream(StreamId streamId, StreamCompletionSignal signal);

  void sendKeepalive(std::unique_ptr<folly::IOBuf> data) override;

  void setResumable(bool resumable);

  bool isPositionAvailable(ResumePosition position);

  void outputFrameOrEnqueue(std::unique_ptr<folly::IOBuf> frame);

  template<typename T>
  void outputFrameOrEnqueue(T&& frame) {
    VLOG(3) << "Out: " << frame;
    outputFrameOrEnqueue(frameSerializer_->serializeOut(std::forward<T>(frame)));
  }

  void requestFireAndForget(Payload request);

  template <typename TFrame>
  bool deserializeFrameOrError(
      TFrame& frame,
      std::unique_ptr<folly::IOBuf> payload) {
    if (frameSerializer_->deserializeFrom(frame, std::move(payload))) {
      return true;
    } else {
      closeWithError(Frame_ERROR::invalidFrame());
      return false;
    }
  }

  template <typename TFrame>
  bool deserializeFrameOrError(
      bool resumable,
      TFrame& frame,
      std::unique_ptr<folly::IOBuf> payload) {
    if (frameSerializer_->deserializeFrom(
            frame, std::move(payload), resumable)) {
      return true;
    } else {
      closeWithError(Frame_ERROR::invalidFrame());
      return false;
    }
  }

  bool resumeFromPositionOrClose(
      ResumePosition serverPosition,
      ResumePosition clientPosition);

  uint32_t getKeepaliveTime() const;
  bool isDisconnectedOrClosed() const;
  bool isClosed() const;

  StreamsFactory& streamsFactory() {
    return streamsFactory_;
  }

  void connectClientSendSetup(
      std::unique_ptr<DuplexConnection> connection,
      SetupParameters setupParams);

  void metadataPush(std::unique_ptr<folly::IOBuf> metadata);

  void tryClientResume(
      const ResumeIdentificationToken& token,
      yarpl::Reference<FrameTransport> frameTransport,
      std::unique_ptr<ClientResumeStatusCallback> resumeCallback);

  void setFrameSerializer(std::unique_ptr<FrameSerializer>);

  RSocketStats& stats() {
    return *stats_;
  }

  std::shared_ptr<RSocketNetworkStats>& networkStats() {
    return networkStats_;
  }

 private:

  bool connect(
      yarpl::Reference<FrameTransport>,
      bool sendingPendingFrames,
      ProtocolVersion protocolVersion);

  /// Performs the same actions as ::endStream without propagating closure
  /// signal to the underlying connection.
  ///
  /// The call is idempotent and returns false iff a stream has not been found.
  bool endStreamInternal(StreamId streamId, StreamCompletionSignal signal);

  /// @{
  /// FrameProcessor methods are implemented with ExecutorBase and automatic
  /// marshaling
  /// onto the right executor to allow DuplexConnection living on a different
  /// executor and calling into ConnectionAutomaton.
  void processFrame(std::unique_ptr<folly::IOBuf>) override;
  void onTerminal(folly::exception_wrapper) override;

  void processFrameImpl(std::unique_ptr<folly::IOBuf>);
  void onTerminalImpl(folly::exception_wrapper);
  /// @}

  void handleConnectionFrame(
      FrameType frameType,
      std::unique_ptr<folly::IOBuf>);
  void handleStreamFrame(
      StreamId streamId,
      FrameType frameType,
      std::unique_ptr<folly::IOBuf> frame);
  void handleUnknownStream(
      StreamId streamId,
      FrameType frameType,
      std::unique_ptr<folly::IOBuf> frame);

  void closeStreams(StreamCompletionSignal);
  void closeFrameTransport(
      folly::exception_wrapper,
      StreamCompletionSignal signal);

  void sendKeepalive(FrameFlags flags, std::unique_ptr<folly::IOBuf> data);

  void resumeFromPosition(ResumePosition position);
  void outputFrame(std::unique_ptr<folly::IOBuf>);

  void debugCheckCorrectExecutor() const;

  void pauseStreams();
  void resumeStreams();

  void writeNewStream(
      StreamId streamId,
      StreamType streamType,
      uint32_t initialRequestN,
      Payload payload,
      bool completed) override;
  void writeRequestN(StreamId streamId, uint32_t n) override;
  void writePayload(StreamId streamId, Payload payload, bool complete) override;
  void writeCloseStream(
      StreamId streamId,
      StreamCompletionSignal signal,
      Payload payload) override;
  void onStreamClosed(StreamId streamId, StreamCompletionSignal signal)
      override;

  bool ensureOrAutodetectFrameSerializer(const folly::IOBuf& firstFrame);

  ReactiveSocketMode mode_;
  bool isResumable_{false};
  bool remoteResumeable_{false};
  bool isClosed_{false};

  std::shared_ptr<ResumeCache> resumeCache_;
  std::shared_ptr<StreamState> streamState_;
  std::shared_ptr<RSocketResponder> requestResponder_;
  yarpl::Reference<FrameTransport> frameTransport_;
  std::unique_ptr<FrameSerializer> frameSerializer_;

  const std::unique_ptr<KeepaliveTimer> keepaliveTimer_;

  std::unique_ptr<ClientResumeStatusCallback> resumeCallback_;

  StreamsFactory streamsFactory_;

  const std::shared_ptr<RSocketStats> stats_;
  std::shared_ptr<RSocketNetworkStats> networkStats_;
  folly::Executor& executor_;
};
}

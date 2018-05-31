// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/statemachine/RSocketStateMachine.h"

#include <folly/ExceptionWrapper.h>
#include <folly/Format.h>
#include <folly/Optional.h>
#include <folly/String.h>
#include <folly/io/async/EventBaseManager.h>

#include "rsocket/DuplexConnection.h"
#include "rsocket/RSocketConnectionEvents.h"
#include "rsocket/RSocketParameters.h"
#include "rsocket/RSocketResponder.h"
#include "rsocket/RSocketStats.h"
#include "rsocket/framing/Frame.h"
#include "rsocket/framing/FrameSerializer.h"
#include "rsocket/framing/FrameTransportImpl.h"
#include "rsocket/internal/ClientResumeStatusCallback.h"
#include "rsocket/internal/ScheduledSubscriber.h"
#include "rsocket/statemachine/ChannelRequester.h"
#include "rsocket/statemachine/ChannelResponder.h"
#include "rsocket/statemachine/RequestResponseRequester.h"
#include "rsocket/statemachine/RequestResponseResponder.h"
#include "rsocket/statemachine/StreamRequester.h"
#include "rsocket/statemachine/StreamResponder.h"
#include "rsocket/statemachine/StreamStateMachineBase.h"

#include "yarpl/flowable/Subscription.h"
#include "yarpl/single/SingleSubscriptions.h"

namespace rsocket {

namespace {

void disconnectError(
    std::shared_ptr<yarpl::flowable::Subscriber<Payload>> subscriber) {
  std::runtime_error exn{"RSocket connection is disconnected or closed"};
  subscriber->onSubscribe(yarpl::flowable::Subscription::create());
  subscriber->onError(std::move(exn));
}

void disconnectError(
    std::shared_ptr<yarpl::single::SingleObserver<Payload>> observer) {
  std::runtime_error exn{"RSocket connection is disconnected or closed"};
  observer->onSubscribe(yarpl::single::SingleSubscriptions::empty());
  observer->onError(std::move(exn));
}

} // namespace

RSocketStateMachine::RSocketStateMachine(
    std::shared_ptr<RSocketResponder> requestResponder,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    RSocketMode mode,
    std::shared_ptr<RSocketStats> stats,
    std::shared_ptr<RSocketConnectionEvents> connectionEvents,
    std::shared_ptr<ResumeManager> resumeManager,
    std::shared_ptr<ColdResumeHandler> coldResumeHandler)
    : RSocketStateMachine(
          std::make_shared<RSocketResponderAdapter>(
              std::move(requestResponder)),
          std::move(keepaliveTimer),
          mode,
          std::move(stats),
          std::move(connectionEvents),
          std::move(resumeManager),
          std::move(coldResumeHandler)) {}

RSocketStateMachine::RSocketStateMachine(
    std::shared_ptr<RSocketResponderCore> requestResponder,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    RSocketMode mode,
    std::shared_ptr<RSocketStats> stats,
    std::shared_ptr<RSocketConnectionEvents> connectionEvents,
    std::shared_ptr<ResumeManager> resumeManager,
    std::shared_ptr<ColdResumeHandler> coldResumeHandler)
    : mode_{mode},
      stats_{stats ? stats : RSocketStats::noop()},
      // Streams initiated by a client MUST use odd-numbered and streams
      // initiated by the server MUST use even-numbered stream identifiers
      nextStreamId_(mode == RSocketMode::CLIENT ? 1 : 2),
      resumeManager_(std::move(resumeManager)),
      requestResponder_{std::move(requestResponder)},
      keepaliveTimer_{std::move(keepaliveTimer)},
      coldResumeHandler_{std::move(coldResumeHandler)},
      connectionEvents_{connectionEvents} {
  CHECK(resumeManager_)
      << "provide ResumeManager::makeEmpty() instead of nullptr";

  // We deliberately do not "open" input or output to avoid having c'tor on the
  // stack when processing any signals from the connection. See ::connect and
  // ::onSubscribe.

  CHECK(requestResponder_);

  stats_->socketCreated();
  VLOG(2) << "Creating RSocketStateMachine";
}

RSocketStateMachine::~RSocketStateMachine() {
  // this destructor can be called from a different thread because the stream
  // automatons destroyed on different threads can be the last ones referencing
  // this.

  VLOG(3) << "~RSocketStateMachine";
  // We rely on SubscriptionPtr and SubscriberPtr to dispatch appropriate
  // terminal signals.
  DCHECK(!resumeCallback_);
  DCHECK(isDisconnected()); // the instance should be closed by via
  // close method
}

void RSocketStateMachine::setResumable(bool resumable) {
  // We should set this flag before we are connected
  DCHECK(isDisconnected());
  isResumable_ = resumable;
}

void RSocketStateMachine::connectServer(
    std::shared_ptr<FrameTransport> frameTransport,
    const SetupParameters& setupParams) {
  setResumable(setupParams.resumable);
  setProtocolVersionOrThrow(setupParams.protocolVersion, frameTransport);
  connect(std::move(frameTransport));
  sendPendingFrames();
}

bool RSocketStateMachine::resumeServer(
    std::shared_ptr<FrameTransport> frameTransport,
    const ResumeParameters& resumeParams) {
  const folly::Optional<int64_t> clientAvailable =
      (resumeParams.clientPosition == kUnspecifiedResumePosition)
      ? folly::none
      : folly::make_optional(
            resumeManager_->impliedPosition() - resumeParams.clientPosition);

  const int64_t serverAvailable =
      resumeManager_->lastSentPosition() - resumeManager_->firstSentPosition();
  const int64_t serverDelta =
      resumeManager_->lastSentPosition() - resumeParams.serverPosition;

  std::runtime_error exn{"Connection being resumed, dropping old connection"};
  disconnect(std::move(exn));
  setProtocolVersionOrThrow(resumeParams.protocolVersion, frameTransport);
  connect(std::move(frameTransport));

  const auto result = resumeFromPositionOrClose(
      resumeParams.serverPosition, resumeParams.clientPosition);

  stats_->serverResume(
      clientAvailable,
      serverAvailable,
      serverDelta,
      result ? RSocketStats::ResumeOutcome::SUCCESS
             : RSocketStats::ResumeOutcome::FAILURE);

  return result;
}

void RSocketStateMachine::connectClient(
    std::shared_ptr<FrameTransport> transport,
    SetupParameters params) {
  auto const version = params.protocolVersion == ProtocolVersion::Unknown
      ? ProtocolVersion::Latest
      : params.protocolVersion;

  setProtocolVersionOrThrow(version, transport);
  setResumable(params.resumable);

  Frame_SETUP frame(
      (params.resumable ? FrameFlags::RESUME_ENABLE : FrameFlags::EMPTY) |
          (params.payload.metadata ? FrameFlags::METADATA : FrameFlags::EMPTY),
      version.major,
      version.minor,
      getKeepaliveTime(),
      Frame_SETUP::kMaxLifetime,
      std::move(params.token),
      std::move(params.metadataMimeType),
      std::move(params.dataMimeType),
      std::move(params.payload));

  // TODO: when the server returns back that it doesn't support resumability, we
  // should retry without resumability

  VLOG(3) << "Out: " << frame;

  connect(std::move(transport));
  // making sure we send setup frame first
  outputFrame(frameSerializer_->serializeOut(std::move(frame)));
  // then the rest of the cached frames will be sent
  sendPendingFrames();
}

void RSocketStateMachine::resumeClient(
    ResumeIdentificationToken token,
    std::shared_ptr<FrameTransport> transport,
    std::unique_ptr<ClientResumeStatusCallback> resumeCallback,
    ProtocolVersion version) {
  // Cold-resumption.  Set the serializer.
  if (!frameSerializer_) {
    CHECK(coldResumeHandler_);
    coldResumeInProgress_ = true;
  }

  setProtocolVersionOrThrow(
      version == ProtocolVersion::Unknown ? ProtocolVersion::Latest : version,
      transport);

  Frame_RESUME resumeFrame(
      std::move(token),
      resumeManager_->impliedPosition(),
      resumeManager_->firstSentPosition(),
      frameSerializer_->protocolVersion());
  VLOG(3) << "Out: " << resumeFrame;

  // Disconnect a previous client if there is one.
  disconnect(std::runtime_error{"Resuming client on a different connection"});

  setResumable(true);
  reconnect(std::move(transport), std::move(resumeCallback));
  outputFrame(frameSerializer_->serializeOut(std::move(resumeFrame)));
}

void RSocketStateMachine::connect(std::shared_ptr<FrameTransport> transport) {
  VLOG(2) << "Connecting to transport " << transport.get();

  CHECK(isDisconnected());
  CHECK(transport);

  // Keep a reference to the argument, make sure the instance survives until
  // setFrameProcessor() returns.  There can be terminating signals processed in
  // that call which will nullify frameTransport_.
  frameTransport_ = transport;

  CHECK(frameSerializer_);
  frameSerializer_->preallocateFrameSizeField() =
      transport->isConnectionFramed();

  if (connectionEvents_) {
    connectionEvents_->onConnected();
  }

  // Keep a reference to this, as processing frames might close this
  // instance.
  const auto copyThis = shared_from_this();
  frameTransport_->setFrameProcessor(copyThis);
  stats_->socketConnected();
}

void RSocketStateMachine::sendPendingFrames() {
  DCHECK(!resumeCallback_);

  StreamsWriterImpl::sendPendingFrames();

  // TODO: turn on only after setup frame was received
  if (keepaliveTimer_) {
    keepaliveTimer_->start(shared_from_this());
  }
}

void RSocketStateMachine::disconnect(folly::exception_wrapper ex) {
  VLOG(2) << "Disconnecting transport";
  if (isDisconnected()) {
    return;
  }

  if (connectionEvents_) {
    connectionEvents_->onDisconnected(ex);
  }

  closeFrameTransport(std::move(ex));

  if (connectionEvents_) {
    connectionEvents_->onStreamsPaused();
  }

  stats_->socketDisconnected();
}

void RSocketStateMachine::close(
    folly::exception_wrapper ex,
    StreamCompletionSignal signal) {
  if (isClosed()) {
    return;
  }

  isClosed_ = true;
  stats_->socketClosed(signal);

  VLOG(6) << "close";

  if (auto resumeCallback = std::move(resumeCallback_)) {
    resumeCallback->onResumeError(
        ConnectionException(ex ? ex.get_exception()->what() : "RS closing"));
  }

  closeStreams(signal);
  closeFrameTransport(ex);

  if (auto connectionEvents = std::move(connectionEvents_)) {
    connectionEvents->onClosed(std::move(ex));
  }

  if (closeCallback_) {
    closeCallback_->remove(*this);
  }
}

void RSocketStateMachine::closeFrameTransport(folly::exception_wrapper ex) {
  if (isDisconnected()) {
    DCHECK(!resumeCallback_);
    return;
  }

  // Stop scheduling keepalives since the socket is now disconnected
  if (keepaliveTimer_) {
    keepaliveTimer_->stop();
  }

  if (auto resumeCallback = std::move(resumeCallback_)) {
    resumeCallback->onResumeError(ConnectionException(
        ex ? ex.get_exception()->what() : "connection closing"));
  }

  // Echo the exception to the frameTransport only if the frameTransport started
  // closing with error.  Otherwise we sent some error frame over the wire and
  // we are closing the transport cleanly.
  if (frameTransport_) {
    frameTransport_->close();
    frameTransport_ = nullptr;
  }
}

void RSocketStateMachine::disconnectOrCloseWithError(Frame_ERROR&& errorFrame) {
  if (isResumable_) {
    std::runtime_error exn{errorFrame.payload_.moveDataToString()};
    disconnect(std::move(exn));
  } else {
    closeWithError(std::move(errorFrame));
  }
}

void RSocketStateMachine::closeWithError(Frame_ERROR&& error) {
  VLOG(3) << "closeWithError "
          << error.payload_.data->cloneAsValue().moveToFbString();

  StreamCompletionSignal signal;
  switch (error.errorCode_) {
    case ErrorCode::INVALID_SETUP:
      signal = StreamCompletionSignal::INVALID_SETUP;
      break;
    case ErrorCode::UNSUPPORTED_SETUP:
      signal = StreamCompletionSignal::UNSUPPORTED_SETUP;
      break;
    case ErrorCode::REJECTED_SETUP:
      signal = StreamCompletionSignal::REJECTED_SETUP;
      break;

    case ErrorCode::CONNECTION_ERROR:
    // StreamCompletionSignal::CONNECTION_ERROR is reserved for
    // frameTransport errors
    // ErrorCode::CONNECTION_ERROR is a normal Frame_ERROR error code which has
    // nothing to do with frameTransport
    case ErrorCode::APPLICATION_ERROR:
    case ErrorCode::REJECTED:
    case ErrorCode::RESERVED:
    case ErrorCode::CANCELED:
    case ErrorCode::INVALID:
    default:
      signal = StreamCompletionSignal::ERROR;
  }

  std::runtime_error exn{error.payload_.cloneDataToString()};
  if (frameSerializer_) {
    outputFrameOrEnqueue(frameSerializer_->serializeOut(std::move(error)));
  }
  close(std::move(exn), signal);
}

void RSocketStateMachine::reconnect(
    std::shared_ptr<FrameTransport> newFrameTransport,
    std::unique_ptr<ClientResumeStatusCallback> resumeCallback) {
  CHECK(newFrameTransport);
  CHECK(resumeCallback);

  CHECK(!resumeCallback_);
  CHECK(isResumable_);
  CHECK(mode_ == RSocketMode::CLIENT);

  // TODO: output frame buffer should not be written to the new connection until
  // we receive resume ok
  resumeCallback_ = std::move(resumeCallback);
  connect(std::move(newFrameTransport));
}

void RSocketStateMachine::requestStream(
    Payload request,
    std::shared_ptr<yarpl::flowable::Subscriber<Payload>> responseSink) {
  if (isDisconnected()) {
    disconnectError(std::move(responseSink));
    return;
  }

  auto const streamId = getNextStreamId();
  auto stateMachine = std::make_shared<StreamRequester>(
      shared_from_this(), streamId, std::move(request));
  auto const result = streams_.emplace(streamId, StreamStateElem{stateMachine});
  DCHECK(result.second);
  stateMachine->subscribe(std::move(responseSink));
}

std::shared_ptr<yarpl::flowable::Subscriber<Payload>>
RSocketStateMachine::requestChannel(
    Payload request,
    bool hasInitialRequest,
    std::shared_ptr<yarpl::flowable::Subscriber<Payload>> responseSink) {
  if (isDisconnected()) {
    disconnectError(std::move(responseSink));
    return nullptr;
  }

  auto const streamId = getNextStreamId();
  std::shared_ptr<ChannelRequester> stateMachine;
  if (hasInitialRequest) {
    stateMachine = std::make_shared<ChannelRequester>(
        std::move(request), shared_from_this(), streamId);
  } else {
    stateMachine =
        std::make_shared<ChannelRequester>(shared_from_this(), streamId);
  }
  auto const result = streams_.emplace(streamId, StreamStateElem{stateMachine});
  DCHECK(result.second);
  stateMachine->subscribe(std::move(responseSink));
  return stateMachine;
}

void RSocketStateMachine::requestResponse(
    Payload request,
    std::shared_ptr<yarpl::single::SingleObserver<Payload>> responseSink) {
  if (isDisconnected()) {
    disconnectError(std::move(responseSink));
    return;
  }

  auto const streamId = getNextStreamId();
  auto stateMachine = std::make_shared<RequestResponseRequester>(
      shared_from_this(), streamId, std::move(request));
  auto const result = streams_.emplace(streamId, StreamStateElem{stateMachine});
  DCHECK(result.second);
  stateMachine->subscribe(std::move(responseSink));
}

void RSocketStateMachine::closeStreams(StreamCompletionSignal signal) {
  while (!streams_.empty()) {
    auto it = streams_.begin();
    auto streamElem = std::move(it->second);
    streams_.erase(it);
    streamElem.stateMachine->endStream(signal);
  }
}

void RSocketStateMachine::processFrame(std::unique_ptr<folly::IOBuf> frame) {
  if (isClosed()) {
    VLOG(4) << "StateMachine has been closed.  Discarding incoming frame";
    return;
  }

  // Necessary in case the only stream state machine closes itself, and takes
  // the RSocketStateMachine with it.
  const auto self = shared_from_this();

  if (!ensureOrAutodetectFrameSerializer(*frame)) {
    constexpr auto msg = "Cannot detect protocol version";
    closeWithError(Frame_ERROR::connectionError(msg));
    return;
  }

  const auto frameType = frameSerializer_->peekFrameType(*frame);
  stats_->frameRead(frameType);

  const auto optStreamId = frameSerializer_->peekStreamId(*frame);
  if (!optStreamId) {
    constexpr auto msg = "Cannot decode stream ID";
    closeWithError(Frame_ERROR::connectionError(msg));
    return;
  }

  const auto frameLength = frame->computeChainDataLength();
  const auto streamId = *optStreamId;
  if (streamId == 0) {
    handleConnectionFrame(frameType, std::move(frame));
  } else if (resumeCallback_) {
    // during the time when we are resuming we are can't receive any other
    // than connection level frames which drives the resumption
    // TODO(lehecka): this assertion should be handled more elegantly using
    // different state machine
    constexpr auto msg = "Received stream frame while resuming";
    LOG(ERROR) << msg;
    closeWithError(Frame_ERROR::connectionError(msg));
    return;
  } else {
    handleStreamFrame(streamId, frameType, std::move(frame));
  }
  resumeManager_->trackReceivedFrame(
      frameLength, frameType, streamId, getConsumerAllowance(streamId));
}

void RSocketStateMachine::onTerminal(folly::exception_wrapper ex) {
  if (isResumable_) {
    disconnect(std::move(ex));
    return;
  }
  const auto termSignal = ex ? StreamCompletionSignal::CONNECTION_ERROR
                             : StreamCompletionSignal::CONNECTION_END;
  close(std::move(ex), termSignal);
}

void RSocketStateMachine::onKeepAlive(
    ResumePosition resumePosition,
    std::unique_ptr<folly::IOBuf> data,
    bool keepAliveRespond) {
  resumeManager_->resetUpToPosition(resumePosition);
  if (mode_ == RSocketMode::SERVER) {
    if (keepAliveRespond) {
      sendKeepalive(FrameFlags::EMPTY, std::move(data));
    } else {
      closeWithError(Frame_ERROR::connectionError("keepalive without flag"));
    }
  } else {
    if (keepAliveRespond) {
      closeWithError(Frame_ERROR::connectionError(
          "client received keepalive with respond flag"));
    } else if (keepaliveTimer_) {
      keepaliveTimer_->keepaliveReceived();
    }
    stats_->keepaliveReceived();
  }
}

void RSocketStateMachine::onMetadataPush(
    std::unique_ptr<folly::IOBuf> metadata) {
  requestResponder_->handleMetadataPush(std::move(metadata));
}

void RSocketStateMachine::onResumeOk(ResumePosition resumePosition) {
  if (!resumeCallback_) {
    constexpr auto msg = "Received RESUME_OK while not resuming";
    closeWithError(Frame_ERROR::connectionError(msg));
    return;
  }

  if (!resumeManager_->isPositionAvailable(resumePosition)) {
    auto const msg = folly::sformat(
        "Client cannot resume, server position {} is not available",
        resumePosition);
    closeWithError(Frame_ERROR::connectionError(msg));
    return;
  }

  if (coldResumeInProgress_) {
    setNextStreamId(resumeManager_->getLargestUsedStreamId());
    for (const auto& it : resumeManager_->getStreamResumeInfos()) {
      const auto streamId = it.first;
      const StreamResumeInfo& streamResumeInfo = it.second;
      if (streamResumeInfo.requester == RequestOriginator::LOCAL &&
          streamResumeInfo.streamType == StreamType::STREAM) {
        auto subscriber = coldResumeHandler_->handleRequesterResumeStream(
            streamResumeInfo.streamToken, streamResumeInfo.consumerAllowance);

        auto stateMachine = std::make_shared<StreamRequester>(
            shared_from_this(), streamId, Payload());
        // Set requested to true (since cold resumption)
        stateMachine->setRequested(streamResumeInfo.consumerAllowance);
        auto const result =
            streams_.emplace(streamId, StreamStateElem{stateMachine});
        DCHECK(result.second);
        stateMachine->subscribe(
            std::make_shared<ScheduledSubscriptionSubscriber<Payload>>(
                std::move(subscriber),
                *folly::EventBaseManager::get()->getEventBase()));
      }
    }
    coldResumeInProgress_ = false;
  }

  auto resumeCallback = std::move(resumeCallback_);
  resumeCallback->onResumeOk();
  resumeFromPosition(resumePosition);
}

void RSocketStateMachine::onError(ErrorCode errorCode, Payload payload) {
  // TODO: handle INVALID_SETUP, UNSUPPORTED_SETUP, REJECTED_SETUP

  if ((errorCode == ErrorCode::CONNECTION_ERROR ||
       errorCode == ErrorCode::REJECTED_RESUME) &&
      resumeCallback_) {
    auto resumeCallback = std::move(resumeCallback_);
    resumeCallback->onResumeError(
        ResumptionException(payload.cloneDataToString()));
    // fall through
  }

  close(
      std::runtime_error(payload.moveDataToString()),
      StreamCompletionSignal::ERROR);
}

void RSocketStateMachine::handleConnectionFrame(
    FrameType frameType,
    std::unique_ptr<folly::IOBuf> payload) {
  switch (frameType) {
    case FrameType::KEEPALIVE: {
      Frame_KEEPALIVE frame;
      if (!deserializeFrameOrError(isResumable_, frame, std::move(payload))) {
        return;
      }
      VLOG(3) << mode_ << " In: " << frame;
      onKeepAlive(
          frame.position_,
          std::move(frame.data_),
          !!(frame.header_.flags & FrameFlags::KEEPALIVE_RESPOND));
      return;
    }
    case FrameType::METADATA_PUSH: {
      Frame_METADATA_PUSH frame;
      if (deserializeFrameOrError(frame, std::move(payload))) {
        VLOG(3) << mode_ << " In: " << frame;
        onMetadataPush(std::move(frame.metadata_));
      }
      return;
    }
    case FrameType::RESUME_OK: {
      Frame_RESUME_OK frame;
      if (!deserializeFrameOrError(frame, std::move(payload))) {
        return;
      }
      VLOG(3) << mode_ << " In: " << frame;
      onResumeOk(frame.position_);
      return;
    }
    case FrameType::ERROR: {
      Frame_ERROR frame;
      if (!deserializeFrameOrError(frame, std::move(payload))) {
        return;
      }
      VLOG(3) << mode_ << " In: " << frame;
      onError(frame.errorCode_, std::move(frame.payload_));
      return;
    }
    case FrameType::SETUP: // this should be processed in SetupResumeAcceptor
    case FrameType::RESUME: // this should be processed in SetupResumeAcceptor
    case FrameType::RESERVED:
    case FrameType::LEASE:
    case FrameType::REQUEST_RESPONSE:
    case FrameType::REQUEST_FNF:
    case FrameType::REQUEST_STREAM:
    case FrameType::REQUEST_CHANNEL:
    case FrameType::REQUEST_N:
    case FrameType::CANCEL:
    case FrameType::PAYLOAD:
    case FrameType::EXT:
    default: {
      auto const msg = folly::sformat(
          "Unexpected {} frame for stream 0", toString(frameType));
      closeWithError(Frame_ERROR::connectionError(msg));
      return;
    }
  }
}

std::shared_ptr<StreamStateMachineBase>
RSocketStateMachine::getStreamStateMachine(StreamId streamId) {
  auto const it = streams_.find(streamId);
  return it != streams_.end() ? it->second.stateMachine : nullptr;
}

void RSocketStateMachine::onStreamRequestN(
    StreamId streamId,
    uint32_t requestN) {
  // we ignore  messages for streams which don't exist
  if (auto stateMachine = getStreamStateMachine(streamId)) {
    stateMachine->handleRequestN(requestN);
  }
}

void RSocketStateMachine::onStreamCancel(StreamId streamId) {
  // we ignore  messages for streams which don't exist
  if (auto stateMachine = getStreamStateMachine(streamId)) {
    stateMachine->handleCancel();
  }
}

void RSocketStateMachine::onStreamError(StreamId streamId, Payload payload) {
  // we ignore  messages for streams which don't exist
  if (auto stateMachine = getStreamStateMachine(streamId)) {
    stateMachine->handleError(std::runtime_error(payload.moveDataToString()));
  }
}

void RSocketStateMachine::onStreamPayload(
    StreamId streamId,
    Payload payload,
    bool flagsFollows,
    bool flagsComplete,
    bool flagsNext) {
  auto const it = streams_.find(streamId);
  if (it == streams_.end()) {
    return;
  }
  // we are purposely making a copy of the reference here to avoid problems with
  // lifetime of the stateMachine when a terminating signal is delivered which
  // will cause the stateMachine to be destroyed while in one of its methods
  auto stateMachine = it->second.stateMachine;
  auto& stateElem = it->second;

  if (flagsFollows && !stateElem.fragmentAccumulator.anyFragments()) {
    // first fragment seen for the frame; copy headers
    stateElem.fragmentAccumulator.flagsComplete = flagsComplete;
    stateElem.fragmentAccumulator.flagsNext = flagsNext;
  }

  if (flagsFollows || stateElem.fragmentAccumulator.anyFragments()) {
    stateElem.fragmentAccumulator.addPayload(std::move(payload));

    // final fragment in the payload, consume the payload
    if (!flagsFollows) {
      stateMachine->handlePayload(
          stateElem.fragmentAccumulator.consumePayload(),
          stateElem.fragmentAccumulator.flagsComplete,
          stateElem.fragmentAccumulator.flagsNext);
    }
  } else {
    // not a fragmented payload
    stateMachine->handlePayload(std::move(payload), flagsComplete, flagsNext);
  }
}

void RSocketStateMachine::handleStreamFrame(
    StreamId streamId,
    FrameType frameType,
    std::unique_ptr<folly::IOBuf> serializedFrame) {
  if (streams_.count(streamId) == 0) {
    handleUnknownStream(streamId, frameType, std::move(serializedFrame));
    return;
  }

  switch (frameType) {
    case FrameType::REQUEST_N: {
      Frame_REQUEST_N frameRequestN;
      if (!deserializeFrameOrError(frameRequestN, std::move(serializedFrame))) {
        return;
      }
      VLOG(3) << mode_ << " In: " << frameRequestN;
      onStreamRequestN(streamId, frameRequestN.requestN_);
      break;
    }
    case FrameType::CANCEL: {
      VLOG(3) << mode_ << " In: " << Frame_CANCEL(streamId);
      onStreamCancel(streamId);
      break;
    }
    case FrameType::PAYLOAD: {
      Frame_PAYLOAD framePayload;
      if (!deserializeFrameOrError(framePayload, std::move(serializedFrame))) {
        return;
      }
      VLOG(3) << mode_ << " In: " << framePayload;
      onStreamPayload(
          streamId,
          std::move(framePayload.payload_),
          framePayload.header_.flagsFollows(),
          framePayload.header_.flagsComplete(),
          framePayload.header_.flagsNext());
      break;
    }
    case FrameType::ERROR: {
      Frame_ERROR frameError;
      if (!deserializeFrameOrError(frameError, std::move(serializedFrame))) {
        return;
      }
      VLOG(3) << mode_ << " In: " << frameError;
      onStreamError(streamId, std::move(frameError.payload_));
      break;
    }
    case FrameType::REQUEST_CHANNEL:
    case FrameType::REQUEST_RESPONSE:
    case FrameType::RESERVED:
    case FrameType::SETUP:
    case FrameType::LEASE:
    case FrameType::KEEPALIVE:
    case FrameType::REQUEST_FNF:
    case FrameType::REQUEST_STREAM:
    case FrameType::METADATA_PUSH:
    case FrameType::RESUME:
    case FrameType::RESUME_OK:
    case FrameType::EXT: {
      auto const msg = folly::sformat(
          "Unexpected {} frame for stream {}", toString(frameType), streamId);
      closeWithError(Frame_ERROR::connectionError(msg));
      break;
    }
    default:
      // Ignore unknown frames for compatibility with future frame types.
      break;
  }
}

void RSocketStateMachine::handleUnknownStream(
    StreamId streamId,
    FrameType frameType,
    std::unique_ptr<folly::IOBuf> serializedFrame) {
  DCHECK_NE(0, streamId);
  if (frameType != FrameType::PAYLOAD) {
    // don't check registerNewPeerStreamId if it's a payload - it may be an
    // additional fragment
    if (frameSerializer_->protocolVersion() > ProtocolVersion{0, 0} &&
        !registerNewPeerStreamId(streamId)) {
      return;
    }
  }

  if (!isNewStreamFrame(frameType) && (frameType != FrameType::PAYLOAD)) {
    auto const msg = folly::sformat(
        "Unexpected frame {} for stream {}", toString(frameType), streamId);
    closeWithError(Frame_ERROR::connectionError(msg));
    return;
  }

  auto saveStreamToken = [&](const Payload& payload) {
    if (coldResumeHandler_) {
      auto streamType = getStreamType(frameType);
      CHECK(streamType != StreamType::FNF);
      auto streamToken = coldResumeHandler_->generateStreamToken(
          payload, streamId, streamType);
      resumeManager_->onStreamOpen(
          streamId, RequestOriginator::REMOTE, streamToken, streamType);
    }
  };

  if (frameType == FrameType::PAYLOAD) {
    Frame_PAYLOAD frame;
    if (!deserializeFrameOrError(frame, std::move(serializedFrame))) {
      return;
    }

    const auto it = streamFragments_.find(streamId);
    if (it == streamFragments_.end()) {
      // Stream may be already closed
      return;
    }

    it->second.addPayload(std::move(frame.payload_));

    // if this is the last fragment in the stream, trigger stream/request
    // creation
    if (!(frame.header_.flags & FrameFlags::FOLLOWS)) {
      auto frag = std::move(it->second);
      streamFragments_.erase(it);
      auto payload = frag.consumePayload();

      switch (frag.streamType) {
        case StreamType::CHANNEL:
          saveStreamToken(payload);
          setupRequestChannel(streamId, frag.requestN, std::move(payload));
          break;

        case StreamType::STREAM:
          saveStreamToken(payload);
          setupRequestStream(streamId, frag.requestN, std::move(payload));
          break;

        case StreamType::REQUEST_RESPONSE:
          saveStreamToken(payload);
          setupRequestResponse(streamId, std::move(payload));
          break;

        case StreamType::FNF:
          setupFireAndForget(streamId, std::move(payload));
          break;

        default:
          CHECK(false) << "Stream cache had invalid stream type "
                       << frag.streamType << " for stream id " << streamId;
      }
    }
  } else if (frameType == FrameType::REQUEST_CHANNEL) {
    Frame_REQUEST_CHANNEL frame;
    if (!deserializeFrameOrError(frame, std::move(serializedFrame))) {
      return;
    }

    if (!!(frame.header_.flags & FrameFlags::FOLLOWS)) {
      handleInitialFollowsFrame(streamId, std::move(frame));
    } else {
      saveStreamToken(frame.payload_);
      setupRequestChannel(streamId, frame.requestN_, std::move(frame.payload_));
    }

  } else if (frameType == FrameType::REQUEST_STREAM) {
    Frame_REQUEST_STREAM frame;
    if (!deserializeFrameOrError(frame, std::move(serializedFrame))) {
      return;
    }
    VLOG(3) << mode_ << " In: " << frame;

    if (!!(frame.header_.flags & FrameFlags::FOLLOWS)) {
      handleInitialFollowsFrame(streamId, std::move(frame));
    } else {
      saveStreamToken(frame.payload_);
      setupRequestStream(streamId, frame.requestN_, std::move(frame.payload_));
    }

  } else if (frameType == FrameType::REQUEST_RESPONSE) {
    Frame_REQUEST_RESPONSE frame;
    if (!deserializeFrameOrError(frame, std::move(serializedFrame))) {
      return;
    }
    VLOG(3) << mode_ << " In: " << frame;

    if (!!(frame.header_.flags & FrameFlags::FOLLOWS)) {
      handleInitialFollowsFrame(streamId, std::move(frame));
    } else {
      saveStreamToken(frame.payload_);
      setupRequestResponse(streamId, std::move(frame.payload_));
    }

  } else if (frameType == FrameType::REQUEST_FNF) {
    Frame_REQUEST_FNF frame;
    if (!deserializeFrameOrError(frame, std::move(serializedFrame))) {
      return;
    }
    VLOG(3) << mode_ << " In: " << frame;
    if (!!(frame.header_.flags & FrameFlags::FOLLOWS)) {
      handleInitialFollowsFrame(streamId, std::move(frame));
    } else {
      // no stream tracking is necessary
      setupFireAndForget(streamId, std::move(frame.payload_));
    }
  }
}

// Called when 'initialFrame' is the first frame for a stream or request, and
// the stream is fragmented.
template <typename FrameType>
void RSocketStateMachine::handleInitialFollowsFrame(
    StreamId streamId,
    FrameType&& initialFrame) {
  const auto it = streamFragments_.find(streamId);
  if (it == streamFragments_.end()) {
    streamFragments_.insert(std::make_pair(
        streamId,
        StreamFragmentAccumulator{initialFrame,
                                  std::move(initialFrame.payload_)}));
  } else {
    auto const msg = folly::sformat(
        "Expected stream {} to already be in fragment cache", streamId);
    closeWithError(Frame_ERROR::connectionError(msg));
  }
}

void RSocketStateMachine::setupFireAndForget(
    StreamId streamId,
    Payload payload) {
  requestResponder_->handleFireAndForget(std::move(payload), streamId);
}

void RSocketStateMachine::setupRequestChannel(
    StreamId streamId,
    uint32_t requestN,
    Payload payload) {
  auto stateMachine = std::make_shared<ChannelResponder>(
      shared_from_this(), streamId, requestN);
  auto const result = streams_.emplace(streamId, StreamStateElem{stateMachine});
  DCHECK(result.second);
  const auto requestSink = requestResponder_->handleRequestChannel(
      std::move(payload), streamId, stateMachine);
  stateMachine->subscribe(requestSink);
}

void RSocketStateMachine::setupRequestStream(
    StreamId streamId,
    uint32_t requestN,
    Payload payload) {
  auto stateMachine =
      std::make_shared<StreamResponder>(shared_from_this(), streamId, requestN);
  auto const result = streams_.emplace(streamId, StreamStateElem{stateMachine});
  DCHECK(result.second);
  requestResponder_->handleRequestStream(
      std::move(payload), streamId, stateMachine);
}

void RSocketStateMachine::setupRequestResponse(
    StreamId streamId,
    Payload payload) {
  auto stateMachine =
      std::make_shared<RequestResponseResponder>(shared_from_this(), streamId);
  auto const result = streams_.emplace(streamId, StreamStateElem{stateMachine});
  DCHECK(result.second);
  requestResponder_->handleRequestResponse(
      std::move(payload), streamId, stateMachine);
}

void RSocketStateMachine::sendKeepalive(std::unique_ptr<folly::IOBuf> data) {
  sendKeepalive(FrameFlags::KEEPALIVE_RESPOND, std::move(data));
}

void RSocketStateMachine::sendKeepalive(
    FrameFlags flags,
    std::unique_ptr<folly::IOBuf> data) {
  Frame_KEEPALIVE pingFrame(
      flags, resumeManager_->impliedPosition(), std::move(data));
  VLOG(3) << mode_ << " Out: " << pingFrame;
  outputFrameOrEnqueue(
      frameSerializer_->serializeOut(std::move(pingFrame), isResumable_));
  stats_->keepaliveSent();
}

bool RSocketStateMachine::isPositionAvailable(ResumePosition position) const {
  return resumeManager_->isPositionAvailable(position);
}

bool RSocketStateMachine::resumeFromPositionOrClose(
    ResumePosition serverPosition,
    ResumePosition clientPosition) {
  DCHECK(!resumeCallback_);
  DCHECK(!isDisconnected());
  DCHECK(mode_ == RSocketMode::SERVER);

  const bool clientPositionExist =
      (clientPosition == kUnspecifiedResumePosition) ||
      clientPosition <= resumeManager_->impliedPosition();

  if (clientPositionExist &&
      resumeManager_->isPositionAvailable(serverPosition)) {
    Frame_RESUME_OK resumeOkFrame{resumeManager_->impliedPosition()};
    VLOG(3) << "Out: " << resumeOkFrame;
    frameTransport_->outputFrameOrDrop(
        frameSerializer_->serializeOut(std::move(resumeOkFrame)));
    resumeFromPosition(serverPosition);
    return true;
  }

  auto const msg = folly::to<std::string>(
      "Cannot resume server, client lastServerPosition=",
      serverPosition,
      " firstClientPosition=",
      clientPosition,
      " is not available. Last reset position is ",
      resumeManager_->firstSentPosition());

  closeWithError(Frame_ERROR::connectionError(msg));
  return false;
}

void RSocketStateMachine::resumeFromPosition(ResumePosition position) {
  DCHECK(!resumeCallback_);
  DCHECK(!isDisconnected());
  DCHECK(resumeManager_->isPositionAvailable(position));

  if (connectionEvents_) {
    connectionEvents_->onStreamsResumed();
  }
  resumeManager_->sendFramesFromPosition(position, *frameTransport_);

  auto frames = consumePendingOutputFrames();
  for (auto& frame : frames) {
    outputFrameOrEnqueue(std::move(frame));
  }

  if (!isDisconnected() && keepaliveTimer_) {
    keepaliveTimer_->start(shared_from_this());
  }
}

bool RSocketStateMachine::shouldQueue() {
  // if we are resuming we cant send any frames until we receive RESUME_OK
  return isDisconnected() || resumeCallback_;
}

void RSocketStateMachine::fireAndForget(Payload request) {
  auto const streamId = getNextStreamId();
  Frame_REQUEST_FNF frame{streamId, FrameFlags::EMPTY, std::move(request)};
  outputFrameOrEnqueue(frameSerializer_->serializeOut(std::move(frame)));
}

void RSocketStateMachine::metadataPush(std::unique_ptr<folly::IOBuf> metadata) {
  Frame_METADATA_PUSH metadataPushFrame{std::move(metadata)};
  outputFrameOrEnqueue(
      frameSerializer_->serializeOut(std::move(metadataPushFrame)));
}

void RSocketStateMachine::outputFrame(std::unique_ptr<folly::IOBuf> frame) {
  DCHECK(!isDisconnected());

  const auto frameType = frameSerializer_->peekFrameType(*frame);
  stats_->frameWritten(frameType);

  if (isResumable_) {
    auto streamIdPtr = frameSerializer_->peekStreamId(*frame);
    CHECK(streamIdPtr) << "Error in serialized frame.";
    resumeManager_->trackSentFrame(
        *frame, frameType, *streamIdPtr, getConsumerAllowance(*streamIdPtr));
  }
  frameTransport_->outputFrameOrDrop(std::move(frame));
}

uint32_t RSocketStateMachine::getKeepaliveTime() const {
  return keepaliveTimer_
      ? static_cast<uint32_t>(keepaliveTimer_->keepaliveTime().count())
      : Frame_SETUP::kMaxKeepaliveTime;
}

bool RSocketStateMachine::isDisconnected() const {
  return !frameTransport_;
}

bool RSocketStateMachine::isClosed() const {
  return isClosed_;
}

void RSocketStateMachine::writeNewStream(
    StreamId streamId,
    StreamType streamType,
    uint32_t initialRequestN,
    Payload payload) {
  if (coldResumeHandler_ && streamType != StreamType::FNF) {
    const auto streamToken =
        coldResumeHandler_->generateStreamToken(payload, streamId, streamType);
    resumeManager_->onStreamOpen(
        streamId, RequestOriginator::LOCAL, streamToken, streamType);
  }

  StreamsWriterImpl::writeNewStream(
      streamId, streamType, initialRequestN, std::move(payload));
}

void RSocketStateMachine::onStreamClosed(StreamId streamId) {
  streams_.erase(streamId);
  streamFragments_.erase(streamId);
  resumeManager_->onStreamClosed(streamId);
}

bool RSocketStateMachine::ensureOrAutodetectFrameSerializer(
    const folly::IOBuf& firstFrame) {
  if (frameSerializer_) {
    return true;
  }

  if (mode_ != RSocketMode::SERVER) {
    // this should never happen as clients are initized with FrameSerializer
    // instance
    DCHECK(false);
    return false;
  }

  auto serializer = FrameSerializer::createAutodetectedSerializer(firstFrame);
  if (!serializer) {
    LOG(ERROR) << "unable to detect protocol version";
    return false;
  }

  VLOG(2) << "detected protocol version" << serializer->protocolVersion();
  frameSerializer_ = std::move(serializer);
  frameSerializer_->preallocateFrameSizeField() =
      frameTransport_ && frameTransport_->isConnectionFramed();

  return true;
}

size_t RSocketStateMachine::getConsumerAllowance(StreamId streamId) const {
  auto const it = streams_.find(streamId);
  return it != streams_.end() ? it->second.stateMachine->getConsumerAllowance()
                              : 0;
}

void RSocketStateMachine::registerCloseCallback(
    RSocketStateMachine::CloseCallback* callback) {
  closeCallback_ = callback;
}

DuplexConnection* RSocketStateMachine::getConnection() {
  return frameTransport_ ? frameTransport_->getConnection() : nullptr;
}

void RSocketStateMachine::setProtocolVersionOrThrow(
    ProtocolVersion version,
    const std::shared_ptr<FrameTransport>& transport) {
  CHECK(version != ProtocolVersion::Unknown);

  // TODO(lehecka): this is a temporary guard to make sure the transport is
  // explicitly closed when exceptions are thrown. The right solution is to
  // automatically close duplex connection in the destructor when unique_ptr
  // is released
  auto transportGuard = folly::makeGuard([&] { transport->close(); });

  if (frameSerializer_) {
    if (frameSerializer_->protocolVersion() != version) {
      // serializer is not interchangeable, it would screw up resumability
      throw std::runtime_error{"Protocol version mismatch"};
    }
  } else {
    auto frameSerializer = FrameSerializer::createFrameSerializer(version);
    if (!frameSerializer) {
      throw std::runtime_error{"Invalid protocol version"};
    }

    frameSerializer_ = std::move(frameSerializer);
    frameSerializer_->preallocateFrameSizeField() =
        frameTransport_ && frameTransport_->isConnectionFramed();
  }

  transportGuard.dismiss();
}

StreamId RSocketStateMachine::getNextStreamId() {
  constexpr auto limit =
      static_cast<uint32_t>(std::numeric_limits<int32_t>::max() - 2);

  auto const streamId = nextStreamId_;
  if (streamId >= limit) {
    throw std::runtime_error{"Ran out of stream IDs"};
  }

  CHECK_EQ(0, streams_.count(streamId))
      << "Next stream ID already exists in the streams map";

  nextStreamId_ += 2;
  return streamId;
}

void RSocketStateMachine::setNextStreamId(StreamId streamId) {
  nextStreamId_ = streamId + 2;
}

bool RSocketStateMachine::registerNewPeerStreamId(StreamId streamId) {
  DCHECK_NE(0, streamId);
  if (nextStreamId_ % 2 == streamId % 2) {
    // if this is an unknown stream to the socket and this socket is generating
    // such stream ids, it is an incoming frame on the stream which no longer
    // exist
    return false;
  }
  if (streamId <= lastPeerStreamId_) {
    // receiving frame for a stream which no longer exists
    return false;
  }
  lastPeerStreamId_ = streamId;
  return true;
}

bool RSocketStateMachine::hasStreams() const {
  return !streams_.empty();
}

} // namespace rsocket

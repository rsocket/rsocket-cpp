// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/ConnectionAutomaton.h"

#include <folly/ExceptionWrapper.h>
#include <folly/MoveWrapper.h>
#include <folly/Optional.h>
#include <folly/String.h>
#include <folly/io/async/EventBase.h>
#include "src/AbstractStreamAutomaton.h"
#include "src/ClientResumeStatusCallback.h"
#include "src/DuplexConnection.h"
#include "src/FrameTransport.h"
#include "src/Stats.h"
#include "src/StreamState.h"

namespace reactivesocket {

ConnectionAutomaton::ConnectionAutomaton(
    folly::Executor& executor,
    StreamAutomatonFactory factory,
    std::shared_ptr<StreamState> streamState,
    std::shared_ptr<RequestHandler> requestHandler,
    ResumeListener resumeListener,
    Stats& stats,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    ReactiveSocketMode mode)
    : ExecutorBase(executor),
      factory_(std::move(factory)),
      streamState_(std::move(streamState)),
      requestHandler_(std::move(requestHandler)),
      stats_(stats),
      mode_(mode),
      resumeListener_(resumeListener),
      keepaliveTimer_(std::move(keepaliveTimer)) {
  // We deliberately do not "open" input or output to avoid having c'tor on the
  // stack when processing any signals from the connection. See ::connect and
  // ::onSubscribe.
  CHECK(streamState_);
}

ConnectionAutomaton::~ConnectionAutomaton() {
  // this destructor can be called from a different thread because the stream
  // automatons destroyed on different threads can be the last ones referencing
  // this.

  VLOG(6) << "~ConnectionAutomaton";
  // We rely on SubscriptionPtr and SubscriberPtr to dispatch appropriate
  // terminal signals.
  DCHECK(!resumeCallback_);
  DCHECK(isDisconnectedOrClosed()); // the instance should be closed by via
  // close() method
}

void ConnectionAutomaton::setResumable(bool resumable) {
  debugCheckCorrectExecutor();
  DCHECK(isDisconnectedOrClosed()); // we allow to set this flag before we are
  // connected
  remoteResumeable_ = isResumable_ = resumable;
}

void ConnectionAutomaton::connect(
    std::shared_ptr<FrameTransport> frameTransport,
    bool sendingPendingFrames) {
  debugCheckCorrectExecutor();
  CHECK(isDisconnectedOrClosed());
  CHECK(frameTransport);
  CHECK(!frameTransport->isClosed());

  frameTransport_ = std::move(frameTransport);

  for (auto& callback : onConnectListeners_) {
    callback();
  }

  // We need to create a hard reference to frameTransport_ to make sure the
  // instance survives until the setFrameProcessor returns. There can be
  // terminating signals processed in that call which will nullify
  // frameTransport_
  auto frameTransportCopy = frameTransport_;
  frameTransport_->setFrameProcessor(shared_from_this());

  if (sendingPendingFrames) {
    DCHECK(!resumeCallback_);
    // we are free to try to send frames again
    // not all frames might be sent if the connection breaks, the rest of them
    // will queue up again
    auto outputFrames = streamState_->moveOutputPendingFrames();
    for (auto& frame : outputFrames) {
      outputFrameOrEnqueue(std::move(frame));
    }

    if (keepaliveTimer_) {
      keepaliveTimer_->start(shared_from_this());
    }
  }
}

std::shared_ptr<FrameTransport> ConnectionAutomaton::detachFrameTransport() {
  debugCheckCorrectExecutor();
  if (isDisconnectedOrClosed()) {
    return nullptr;
  }

  frameTransport_->setFrameProcessor(nullptr);
  return std::move(frameTransport_);
}

void ConnectionAutomaton::disconnect(folly::exception_wrapper ex) {
  debugCheckCorrectExecutor();
  VLOG(6) << "disconnect";
  if (isDisconnectedOrClosed()) {
    return;
  }

  for (auto& callback : onDisconnectListeners_) {
    callback(ex);
  }

  closeFrameTransport(std::move(ex));
  pauseStreams();
  stats_.socketDisconnected();
}

void ConnectionAutomaton::close() {
  close(folly::exception_wrapper(), StreamCompletionSignal::SOCKET_CLOSED);
}

void ConnectionAutomaton::close(
    folly::exception_wrapper ex,
    StreamCompletionSignal signal) {
  debugCheckCorrectExecutor();
  VLOG(6) << "close";

  if (resumeCallback_) {
    resumeCallback_->onResumeError(
        std::runtime_error(ex ? ex.what().c_str() : "RS closing"));
    resumeCallback_.reset();
  }

  if (!isClosed_) {
    isClosed_ = true;
    stats_.socketClosed(signal);
  }

  onConnectListeners_.clear();
  onDisconnectListeners_.clear();
  auto onCloseListeners = std::move(onCloseListeners_);
  for (auto& callback : onCloseListeners) {
    callback(ex);
  }

  closeStreams(signal);
  closeFrameTransport(std::move(ex));
}

void ConnectionAutomaton::closeFrameTransport(folly::exception_wrapper ex) {
  if (isDisconnectedOrClosed()) {
    DCHECK(!resumeCallback_);
    return;
  }

  // Stop scheduling keepalives since the socket is now disconnected
  if (keepaliveTimer_) {
    keepaliveTimer_->stop();
  }

  if (resumeCallback_) {
    resumeCallback_->onConnectionError(
        std::runtime_error(ex ? ex.what().c_str() : "connection closing"));
    resumeCallback_.reset();
  }

  frameTransport_->close(std::move(ex));
  frameTransport_ = nullptr;
}

void ConnectionAutomaton::closeWithError(Frame_ERROR&& error) {
  debugCheckCorrectExecutor();
  VLOG(4) << "closeWithError "
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
      signal = StreamCompletionSignal::CONNECTION_ERROR;
      break;

    case ErrorCode::APPLICATION_ERROR:
    case ErrorCode::REJECTED:
    case ErrorCode::RESERVED:
    case ErrorCode::CANCELED:
    case ErrorCode::INVALID:
    default:
      signal = StreamCompletionSignal::ERROR;
  }

  outputFrameOrEnqueue(error.serializeOut());
  close(folly::exception_wrapper(), signal);
}

void ConnectionAutomaton::reconnect(
    std::shared_ptr<FrameTransport> newFrameTransport,
    std::unique_ptr<ClientResumeStatusCallback> resumeCallback) {
  debugCheckCorrectExecutor();
  CHECK(newFrameTransport);
  CHECK(resumeCallback);
  CHECK(!resumeCallback_);
  CHECK(isResumable_);
  CHECK(mode_ == ReactiveSocketMode::CLIENT);

  disconnect(std::runtime_error("reconnecting to a different connection"));
  // TODO: output frame buffer should not be written to the new connection until
  // we receive resume ok
  resumeCallback_ = std::move(resumeCallback);
  connect(std::move(newFrameTransport), false);
}

void ConnectionAutomaton::addStream(
    StreamId streamId,
    std::shared_ptr<AbstractStreamAutomaton> automaton) {
  debugCheckCorrectExecutor();
  auto result = streamState_->streams_.emplace(streamId, std::move(automaton));
  (void)result;
  assert(result.second);
}

void ConnectionAutomaton::endStream(
    StreamId streamId,
    StreamCompletionSignal signal) {
  debugCheckCorrectExecutor();
  VLOG(6) << "endStream";
  // The signal must be idempotent.
  if (!endStreamInternal(streamId, signal)) {
    return;
  }
  // TODO(stupaq): handle connection-level errors
  assert(
      signal == StreamCompletionSignal::GRACEFUL ||
      signal == StreamCompletionSignal::ERROR);
}

bool ConnectionAutomaton::endStreamInternal(
    StreamId streamId,
    StreamCompletionSignal signal) {
  VLOG(6) << "endStreamInternal";
  auto it = streamState_->streams_.find(streamId);
  if (it == streamState_->streams_.end()) {
    // Unsubscribe handshake initiated by the connection, we're done.
    return false;
  }
  // Remove from the map before notifying the automaton.
  auto automaton = std::move(it->second);
  streamState_->streams_.erase(it);
  automaton->endStream(signal);
  return true;
}

void ConnectionAutomaton::closeStreams(StreamCompletionSignal signal) {
  // Close all streams.
  while (!streamState_->streams_.empty()) {
    auto oldSize = streamState_->streams_.size();
    auto result =
        endStreamInternal(streamState_->streams_.begin()->first, signal);
    (void)oldSize;
    (void)result;
    // TODO(stupaq): what kind of a user action could violate these
    // assertions?
    assert(result);
    assert(streamState_->streams_.size() == oldSize - 1);
  }
}

void ConnectionAutomaton::pauseStreams() {
  for (auto& streamKV : streamState_->streams_) {
    streamKV.second->pauseStream(*requestHandler_);
  }
}

void ConnectionAutomaton::resumeStreams() {
  for (auto& streamKV : streamState_->streams_) {
    streamKV.second->resumeStream(*requestHandler_);
  }
}

void ConnectionAutomaton::processFrame(std::unique_ptr<folly::IOBuf> frame) {
  auto thisPtr = this->shared_from_this();
  auto movedFrame = folly::makeMoveWrapper(frame);
  runInExecutor([thisPtr, movedFrame]() mutable {
    thisPtr->processFrameImpl(movedFrame.move());
  });
}

void ConnectionAutomaton::processFrameImpl(
    std::unique_ptr<folly::IOBuf> frame) {
  auto frameType = FrameHeader::peekType(*frame);

  std::stringstream ss;
  ss << frameType;

  stats_.frameRead(ss.str());

  // TODO(tmont): If a frame is invalid, it will still be tracked. However, we
  // actually want that. We want to keep
  // each side in sync, even if a frame is invalid.
  streamState_->resumeTracker_.trackReceivedFrame(*frame);

  auto streamIdPtr = FrameHeader::peekStreamId(*frame);
  if (!streamIdPtr) {
    // Failed to deserialize the frame.
    closeWithError(Frame_ERROR::connectionError("invalid frame"));
    return;
  }
  auto streamId = *streamIdPtr;
  if (streamId == 0) {
    onConnectionFrame(std::move(frame));
    return;
  }

  // during the time when we are resuming we are can't receive any other
  // than connection level frames which drives the resumption
  // TODO(lehecka): this assertion should be handled more elegantly using
  // different state machine
  if (resumeCallback_) {
    LOG(ERROR) << "received stream frames during resumption";
    closeWithError(Frame_ERROR::unexpectedFrame());
    return;
  }

  auto it = streamState_->streams_.find(streamId);
  if (it == streamState_->streams_.end()) {
    handleUnknownStream(streamId, std::move(frame));
    return;
  }
  auto automaton = it->second;
  // Can deliver the frame.
  automaton->onNextFrame(std::move(frame));
}

void ConnectionAutomaton::onTerminal(
    folly::exception_wrapper ex,
    StreamCompletionSignal signal) {
  auto thisPtr = this->shared_from_this();
  auto movedEx = folly::makeMoveWrapper(ex);
  runInExecutor([thisPtr, movedEx, signal]() mutable {
    thisPtr->onTerminalImpl(movedEx.move(), signal);
  });
}

void ConnectionAutomaton::onTerminalImpl(
    folly::exception_wrapper ex,
    StreamCompletionSignal signal) {
  if (isResumable_) {
    disconnect(std::move(ex));
  } else {
    close(std::move(ex), signal);
  }
}

void ConnectionAutomaton::onConnectionFrame(
    std::unique_ptr<folly::IOBuf> payload) {
  auto type = FrameHeader::peekType(*payload);
  switch (type) {
    case FrameType::KEEPALIVE: {
      Frame_KEEPALIVE frame;
      if (!deserializeFrameOrError(
              remoteResumeable_, frame, std::move(payload))) {
        return;
      }
      if (mode_ == ReactiveSocketMode::SERVER) {
        if (frame.header_.flags_ & FrameFlags_KEEPALIVE_RESPOND) {
          frame.header_.flags_ &= ~(FrameFlags_KEEPALIVE_RESPOND);
          outputFrameOrEnqueue(frame.serializeOut(remoteResumeable_));
        } else {
          closeWithError(
              Frame_ERROR::connectionError("keepalive without flag"));
        }

        streamState_->resumeCache_.resetUpToPosition(frame.position_);
      } else {
        if (frame.header_.flags_ & FrameFlags_KEEPALIVE_RESPOND) {
          closeWithError(Frame_ERROR::connectionError(
              "client received keepalive with respond flag"));
        } else if (keepaliveTimer_) {
          keepaliveTimer_->keepaliveReceived();
        }
      }
      return;
    }
    case FrameType::SETUP: {
      // TODO(tmont): check for ENABLE_RESUME and make sure isResumable_ is true
      Frame_SETUP frame;
      if (!deserializeFrameOrError(frame, payload->clone())) {
        return;
      }
      if (frame.header_.flags_ & FrameFlags_RESUME_ENABLE) {
        remoteResumeable_ = true;
      } else {
        remoteResumeable_ = false;
      }
      // TODO(blom, t15719366): Don't pass the full frame here
      factory_(*this, 0, std::move(payload));
      return;
    }
    case FrameType::METADATA_PUSH: {
      factory_(*this, 0, std::move(payload));
      return;
    }
    case FrameType::RESUME: {
      if (mode_ == ReactiveSocketMode::SERVER && isResumable_) {
        factory_(*this, 0, std::move(payload));
        //      Frame_RESUME frame;
        //      if (!deserializeFrameOrError(frame, std::move(payload))) {
        //        return;
        //      }
        //      bool canResume = false;
        //
        //      if (isServer_ && isResumable_) {
        //        auto streamState = resumeListener_(frame.token_);
        //        if (nullptr != streamState) {
        //          canResume = true;
        //          useStreamState(streamState);
        //        }
        //      }
        //
        //      if (canResume) {
        //        outputFrameOrEnqueue(
        //            Frame_RESUME_OK(streamState_->resumeTracker_.impliedPosition())
        //                .serializeOut());
        //        for (auto it : streamState_->streams_) {
        //          const StreamId streamId = it.first;
        //
        //          if (streamState_->resumeCache_.isPositionAvailable(
        //                  frame.position_, streamId)) {
        //            it.second->onCleanResume();
        //          } else {
        //            it.second->onDirtyResume();
        //          }
        //        }
      } else {
        closeWithError(
            Frame_ERROR::connectionError("RS not resumable. Can not resume"));
      }
      return;
    }
    case FrameType::RESUME_OK: {
      Frame_RESUME_OK frame;
      if (!deserializeFrameOrError(frame, std::move(payload))) {
        return;
      }
      if (resumeCallback_) {
        if (streamState_->resumeCache_.isPositionAvailable(frame.position_)) {
          resumeCallback_->onResumeOk();
          resumeCallback_.reset();
          resumeFromPosition(frame.position_);
        } else {
          closeWithError(Frame_ERROR::connectionError("can not resume"));
        }
      } else {
        closeWithError(Frame_ERROR::unexpectedFrame());
      }
      return;
    }
    case FrameType::ERROR: {
      Frame_ERROR frame;
      if (!deserializeFrameOrError(frame, std::move(payload))) {
        return;
      }

      // TODO: handle INVALID_SETUP, UNSUPPORTED_SETUP, REJECTED_SETUP

      if (frame.errorCode_ == ErrorCode::CONNECTION_ERROR && resumeCallback_) {
        resumeCallback_->onResumeError(
            std::runtime_error(frame.payload_.moveDataToString()));
        resumeCallback_.reset();
        // fall through
      }

      close(
          std::runtime_error(frame.payload_.moveDataToString()),
          StreamCompletionSignal::ERROR);
      return;
    }
    case FrameType::RESERVED:
    case FrameType::LEASE:
    case FrameType::REQUEST_RESPONSE:
    case FrameType::REQUEST_FNF:
    case FrameType::REQUEST_STREAM:
    case FrameType::REQUEST_SUB:
    case FrameType::REQUEST_CHANNEL:
    case FrameType::REQUEST_N:
    case FrameType::CANCEL:
    case FrameType::RESPONSE:
    default:
      closeWithError(Frame_ERROR::unexpectedFrame());
      return;
  }
}

void ConnectionAutomaton::handleUnknownStream(
    StreamId streamId,
    std::unique_ptr<folly::IOBuf> payload) {
  factory_(*this, streamId, std::move(payload));
}
/// @}

void ConnectionAutomaton::sendKeepalive() {
  debugCheckCorrectExecutor();
  Frame_KEEPALIVE pingFrame(
      FrameFlags_KEEPALIVE_RESPOND,
      streamState_->resumeTracker_.impliedPosition(),
      folly::IOBuf::create(0));
  outputFrameOrEnqueue(pingFrame.serializeOut(remoteResumeable_));
}

Frame_RESUME ConnectionAutomaton::createResumeFrame(
    const ResumeIdentificationToken& token) const {
  return Frame_RESUME(token, streamState_->resumeTracker_.impliedPosition());
}

bool ConnectionAutomaton::isPositionAvailable(ResumePosition position) {
  debugCheckCorrectExecutor();
  return streamState_->resumeCache_.isPositionAvailable(position);
}

// ResumePosition ConnectionAutomaton::positionDifference(
//    ResumePosition position) {
//  return streamState_->resumeCache_.position() - position;
//}

bool ConnectionAutomaton::resumeFromPositionOrClose(ResumePosition position) {
  debugCheckCorrectExecutor();
  DCHECK(!resumeCallback_);
  DCHECK(!isDisconnectedOrClosed());

  if (streamState_->resumeCache_.isPositionAvailable(position)) {
    frameTransport_->outputFrameOrEnqueue(
        Frame_RESUME_OK(streamState_->resumeTracker_.impliedPosition())
            .serializeOut());
    resumeFromPosition(position);
    return true;
  } else {
    closeWithError(
        Frame_ERROR::connectionError("Position not available. Can not resume"));
    return false;
  }
}

void ConnectionAutomaton::resumeFromPosition(ResumePosition position) {
  DCHECK(!resumeCallback_);
  DCHECK(!isDisconnectedOrClosed());
  DCHECK(streamState_->resumeCache_.isPositionAvailable(position));

  resumeStreams();
  streamState_->resumeCache_.sendFramesFromPosition(position, *frameTransport_);

  for (auto& frame : streamState_->moveOutputPendingFrames()) {
    outputFrameOrEnqueue(std::move(frame));
  }

  if (!isDisconnectedOrClosed() && keepaliveTimer_) {
    keepaliveTimer_->start(shared_from_this());
  }
}

void ConnectionAutomaton::outputFrameOrEnqueue(
    std::unique_ptr<folly::IOBuf> frame) {
  debugCheckCorrectExecutor();
  // if we are resuming we cant send any frames until we receive RESUME_OK
  if (!isDisconnectedOrClosed() && !resumeCallback_) {
    outputFrame(std::move(frame));
  } else {
    streamState_->enqueueOutputPendingFrame(std::move(frame));
  }
}

void ConnectionAutomaton::outputFrame(std::unique_ptr<folly::IOBuf> frame) {
  DCHECK(!isDisconnectedOrClosed());

  std::stringstream ss;
  ss << FrameHeader::peekType(*frame);
  stats_.frameWritten(ss.str());

  streamState_->resumeCache_.trackSentFrame(*frame);
  frameTransport_->outputFrameOrEnqueue(std::move(frame));
}

void ConnectionAutomaton::useStreamState(
    std::shared_ptr<StreamState> streamState) {
  CHECK(streamState);
  if (mode_ == ReactiveSocketMode::SERVER && isResumable_) {
    streamState_.swap(streamState);
  }
}

uint32_t ConnectionAutomaton::getKeepaliveTime() const {
  debugCheckCorrectExecutor();
  return keepaliveTimer_
      ? static_cast<uint32_t>(keepaliveTimer_->keepaliveTime().count())
      : std::numeric_limits<uint32_t>::max();
}

bool ConnectionAutomaton::isDisconnectedOrClosed() const {
  return !frameTransport_;
}

bool ConnectionAutomaton::isClosed() const {
  return isClosed_;
}

DuplexConnection* ConnectionAutomaton::duplexConnection() const {
  debugCheckCorrectExecutor();
  return frameTransport_ ? frameTransport_->duplexConnection() : nullptr;
}

void ConnectionAutomaton::debugCheckCorrectExecutor() const {
  DCHECK(
      !dynamic_cast<folly::EventBase*>(&executor()) ||
      dynamic_cast<folly::EventBase*>(&executor())->isInEventBaseThread());
}

void ConnectionAutomaton::addConnectedListener(std::function<void()> listener) {
  CHECK(listener);
  onConnectListeners_.push_back(std::move(listener));
}

void ConnectionAutomaton::addDisconnectedListener(ErrorCallback listener) {
  CHECK(listener);
  onDisconnectListeners_.push_back(std::move(listener));
}

void ConnectionAutomaton::addClosedListener(ErrorCallback listener) {
  CHECK(listener);
  onCloseListeners_.push_back(std::move(listener));
}

} // reactivesocket

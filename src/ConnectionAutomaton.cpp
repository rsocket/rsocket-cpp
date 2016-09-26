// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/ConnectionAutomaton.h"

#include <limits>

#include <folly/ExceptionWrapper.h>
#include <folly/Optional.h>
#include <folly/String.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>
#include <iostream>
#include <sstream>

#include "src/AbstractStreamAutomaton.h"
#include "src/DuplexConnection.h"
#include "src/Frame.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

ConnectionAutomaton::ConnectionAutomaton(
    std::unique_ptr<DuplexConnection> connection,
    StreamAutomatonFactory factory,
    ResumeListener resumeListener,
    Stats& stats,
    bool isServer)
    : connection_(std::move(connection)),
      factory_(std::move(factory)),
      stats_(stats),
      isServer_(isServer),
      isResumable_(true),
      resumeTracker_(new ResumeTracker()),
      resumeCache_(new ResumeCache()),
      resumeListener_(resumeListener) {
  // We deliberately do not "open" input or output to avoid having c'tor on the
  // stack when processing any signals from the connection. See ::connect and
  // ::onSubscribe.
}

void ConnectionAutomaton::connect() {
  connectionOutput_.reset(&connection_->getOutput());
  connectionOutput_.get()->onSubscribe(*this);
  // This may call ::onSubscribe in-line, which calls ::request on the provided
  // subscription, which might deliver frames in-line.
  connection_->setInput(*this);

  stats_.socketCreated();
}

void ConnectionAutomaton::disconnect() {
  VLOG(6) << "disconnect";

  if (!connectionOutput_) {
    return;
  }

  LOG_IF(WARNING, !pendingWrites_.empty())
      << "disconnecting with pending writes (" << pendingWrites_.size() << ")";

  // Send terminal signals to the DuplexConnection's input and output before
  // tearing it down. We must do this per DuplexConnection specification (see
  // interface definition).
  connectionOutput_.onComplete();
  connectionInputSub_.cancel();
  connection_.reset();

  stats_.socketClosed();

  for (auto closeListener : closeListeners_) {
    closeListener();
  }
}

void ConnectionAutomaton::reconnect(
    std::unique_ptr<DuplexConnection> newConnection) {
  disconnect();
  connection_ = std::move(newConnection);
  connect();
}

ConnectionAutomaton::~ConnectionAutomaton() {
  VLOG(6) << "~ConnectionAutomaton";
  // We rely on SubscriptionPtr and SubscriberPtr to dispatch appropriate
  // terminal signals.
}

void ConnectionAutomaton::addStream(
    StreamId streamId,
    AbstractStreamAutomaton& automaton) {
  auto result = streams_.emplace(streamId, &automaton);
  (void)result;
  assert(result.second);
}

void ConnectionAutomaton::endStream(
    StreamId streamId,
    StreamCompletionSignal signal) {
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
  auto it = streams_.find(streamId);
  if (it == streams_.end()) {
    // Unsubscribe handshake initiated by the connection, we're done.
    return false;
  }
  // Remove from the map before notifying the automaton.
  auto automaton = it->second;
  streams_.erase(it);
  automaton->endStream(signal);
  return true;
}

/// @{
void ConnectionAutomaton::onSubscribe(Subscription& subscription) {
  assert(!connectionInputSub_);
  connectionInputSub_.reset(&subscription);
  // This may result in signals being issued by the connection in-line, see
  // ::connect.
  connectionInputSub_.request(std::numeric_limits<size_t>::max());
}

void ConnectionAutomaton::onNext(std::unique_ptr<folly::IOBuf> frame) {
  auto frameType = FrameHeader::peekType(*frame);

  std::stringstream ss;
  ss << frameType;

  stats_.frameRead(ss.str());

  // TODO(tmont): If a frame is invalid, it will still be tracked. However, we
  // actually want that. We want to keep
  // each side in sync, even if a frame is invalid.
  resumeTracker_->trackReceivedFrame(*frame);

  auto streamIdPtr = FrameHeader::peekStreamId(*frame);
  if (!streamIdPtr) {
    // Failed to deserialize the frame.
    // TODO(stupaq): handle connection-level error
    assert(false);
    return;
  }
  auto streamId = *streamIdPtr;
  if (streamId == 0) {
    onConnectionFrame(std::move(frame));
    return;
  }
  auto it = streams_.find(streamId);
  if (it == streams_.end()) {
    handleUnknownStream(streamId, std::move(frame));
    return;
  }
  auto automaton = it->second;
  // Can deliver the frame.
  automaton->onNextFrame(std::move(frame));
}

void ConnectionAutomaton::onComplete() {
  onTerminal(folly::exception_wrapper());
}

void ConnectionAutomaton::onError(folly::exception_wrapper ex) {
  onTerminal(std::move(ex));
}

void ConnectionAutomaton::onConnectionFrame(
    std::unique_ptr<folly::IOBuf> payload) {
  auto type = FrameHeader::peekType(*payload);

  switch (type) {
    case FrameType::KEEPALIVE: {
      Frame_KEEPALIVE frame;
      if (frame.deserializeFrom(std::move(payload))) {
        if (isServer_) {
          if (frame.header_.flags_ & FrameFlags_KEEPALIVE_RESPOND) {
            frame.header_.flags_ &= ~(FrameFlags_KEEPALIVE_RESPOND);
            outputFrameOrEnqueue(frame.serializeOut());
          } else {
            outputFrameOrEnqueue(
                Frame_ERROR::invalid("keepalive without flag").serializeOut());
            disconnect();
          }
        }
        // TODO(yschimke) client *should* check the respond flag
      } else {
        outputFrameOrEnqueue(Frame_ERROR::unexpectedFrame().serializeOut());
        disconnect();
      }
      return;
    }
    case FrameType::SETUP: {
      // TODO(tmont): check for ENABLE_RESUME and make sure isResumable_ is true
      // TODO(tmont): figure out how best to pass in ResumeIdentificationToken

      if (!factory_(0, std::move(payload))) {
        assert(false);
      }
      return;
    }
    case FrameType::METADATA_PUSH: {
      if (!factory_(0, std::move(payload))) {
        assert(false);
      }
      return;
    }
    case FrameType::RESUME: {
      Frame_RESUME frame;
      if (frame.deserializeFrom(std::move(payload))) {
        bool canResume = false;

        if (isServer_ && isResumable_) {
          // find old ConnectionAutmaton via calling listener.
          // Application will call resumeFromAutomaton to setup streams and
          // resume information
          canResume = resumeListener_(frame.token_, frame.position_);
        }

        if (canResume) {
          outputFrameOrEnqueue(
              Frame_RESUME_OK(resumeTracker_->impliedPosition())
                  .serializeOut());
          resumeCache_->retransmitFromPosition(frame.position_, *this);
        } else {
          outputFrameOrEnqueue(
              Frame_ERROR::canNotResume("can not resume").serializeOut());
          disconnect();
        }
      } else {
        outputFrameOrEnqueue(Frame_ERROR::unexpectedFrame().serializeOut());
        disconnect();
      }
      return;
    }
    case FrameType::RESUME_OK: {
      Frame_RESUME_OK frame;
      if (frame.deserializeFrom(std::move(payload))) {
        if (!isServer_ && isResumable_ &&
            resumeCache_->isPositionAvailable(frame.position_)) {
          resumeCache_->retransmitFromPosition(frame.position_, *this);
        } else {
          outputFrameOrEnqueue(
              Frame_ERROR::canNotResume("can not resume").serializeOut());
          disconnect();
        }
      } else {
        outputFrameOrEnqueue(Frame_ERROR::unexpectedFrame().serializeOut());
        disconnect();
      }
      return;
    }
    default:
      outputFrameOrEnqueue(Frame_ERROR::unexpectedFrame().serializeOut());
      disconnect();
      return;
  }
}
/// @}

void ConnectionAutomaton::onTerminal(folly::exception_wrapper ex) {
  VLOG(6) << "onTerminal";
  // TODO(stupaq): we should rather use error codes that we do understand
  // instead of exceptions we have no idea about
  auto signal = ex ? StreamCompletionSignal::CONNECTION_ERROR
                   : StreamCompletionSignal::CONNECTION_END;

  if (ex) {
    VLOG(1) << signal << " from " << ex.what();
  }

  // Close all streams.
  while (!streams_.empty()) {
    auto oldSize = streams_.size();
    auto result = endStreamInternal(streams_.begin()->first, signal);
    (void)oldSize;
    (void)result;
    // TODO(stupaq): what kind of a user action could violate these
    // assertions?
    assert(result);
    assert(streams_.size() == oldSize - 1);
  }

  disconnect();
}

/// @{
void ConnectionAutomaton::request(size_t n) {
  if (writeAllowance_.release(n) > 0) {
    // There are no pending writes or we already have this method on the
    // stack.
    return;
  }
  drainOutputFramesQueue();
}

void ConnectionAutomaton::cancel() {
  VLOG(6) << "cancel";

  pendingWrites_.clear();

  disconnect();
}
/// @}

/// @{
void ConnectionAutomaton::handleUnknownStream(
    StreamId streamId,
    std::unique_ptr<folly::IOBuf> payload) {
  // TODO(stupaq): there are some rules about monotonically increasing stream
  // IDs -- let's forget about them for a moment
  if (!factory_(streamId, std::move(payload))) {
    outputFrameOrEnqueue(
        Frame_ERROR::invalid("unknown stream " + folly::to<std::string>(streamId))
            .serializeOut());
    disconnect();
  }
}
/// @}

void ConnectionAutomaton::sendKeepalive() {
  Frame_KEEPALIVE pingFrame(
      FrameFlags_KEEPALIVE_RESPOND, folly::IOBuf::create(0));
  outputFrameOrEnqueue(pingFrame.serializeOut());
}

void ConnectionAutomaton::sendResume(const ResumeIdentificationToken& token) {
  Frame_RESUME resumeFrame(token, resumeTracker_->impliedPosition());
  outputFrameOrEnqueue(resumeFrame.serializeOut());
}

bool ConnectionAutomaton::isPositionAvailable(ResumePosition position) {
  return resumeCache_->isPositionAvailable(position);
}

ResumePosition ConnectionAutomaton::positionDifference(
    ResumePosition position) {
  return resumeCache_->position() - position;
}

void ConnectionAutomaton::onClose(ConnectionCloseListener listener) {
  closeListeners_.push_back(listener);
}

void ConnectionAutomaton::outputFrameOrEnqueue(
    std::unique_ptr<folly::IOBuf> frame) {
  if (!connectionOutput_) {
    return; // RS destructor has disconnected us from the DuplexConnection
  }

  drainOutputFramesQueue();
  if (pendingWrites_.empty() && writeAllowance_.tryAcquire()) {
    outputFrame(std::move(frame));
  } else {
    // We either have no allowance to perform the operation, or the queue has
    // not been drained (e.g. we're looping in ::request).
    pendingWrites_.emplace_back(std::move(frame));
  }
}

void ConnectionAutomaton::drainOutputFramesQueue() {
  // Drain the queue or the allowance.
  while (!pendingWrites_.empty() && writeAllowance_.tryAcquire()) {
    auto frame = std::move(pendingWrites_.front());
    pendingWrites_.pop_front();
    outputFrame(std::move(frame));
  }
}

void ConnectionAutomaton::outputFrame(
    std::unique_ptr<folly::IOBuf> outputFrame) {
  std::stringstream ss;
  ss << FrameHeader::peekType(*outputFrame);

  stats_.frameWritten(ss.str());

  resumeCache_->trackAndCacheSentFrame(*outputFrame);
  connectionOutput_.onNext(std::move(outputFrame));
}

void ConnectionAutomaton::resumeFromAutomaton(
    ConnectionAutomaton& oldAutomaton) {
  if (isServer_ && isResumable_) {
    streams_ = std::move(oldAutomaton.streams_);
    factory_ = oldAutomaton.factory_;

    resumeTracker_ = std::move(oldAutomaton.resumeTracker_);
    resumeCache_ = std::move(oldAutomaton.resumeCache_);
  }
}
}

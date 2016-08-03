// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include <reactive-streams/utilities/AllowanceSemaphore.h>
#include <reactive-streams/utilities/SmartPointers.h>
#include "src/AbstractStreamAutomaton.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/mixins/ConsumerMixin.h"
#include "src/mixins/ExecutorMixin.h"
#include "src/mixins/LoggingMixin.h"
#include "src/mixins/MemoryMixin.h"
#include "src/mixins/MixinTerminator.h"
#include "src/mixins/SourceIfMixin.h"
#include "src/mixins/StreamIfMixin.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

enum class StreamCompletionSignal;

/// Implementation of stream automaton that represents a Subscription requester.
template <class T>
class StreamSubscriptionRequesterBase
    : public LoggingMixin<ConsumerMixin<Frame_RESPONSE, MixinTerminator>> {
  using Base = LoggingMixin<ConsumerMixin<Frame_RESPONSE, MixinTerminator>>;

 public:
  using Base::Base;

  /// Degenerate form of the Subscriber interface -- only one request payload
  /// will be sent to the server.
  void onNext(Payload);

  /// @{
  /// A Subscriber interface to control ingestion of response payloads.
  void request(size_t);

  void cancel();
  /// @}

 protected:
  /// @{
  void endStream(StreamCompletionSignal);

  /// Not all frames are intercepted, some just pass through.
  using Base::onNextFrame;

  void onNextFrame(Frame_RESPONSE&);

  void onNextFrame(Frame_ERROR&);

  std::ostream& logPrefix(std::ostream& os);
  /// @}

  /// State of the Subscription requester.
  enum class State : uint8_t {
    NEW,
    REQUESTED,
    CLOSED,
  } state_{State::NEW};

 private:
  /// An allowance accumulated before the stream is initialised.
  /// Remaining part of the allowance is forwarded to the ConsumerMixin.
  reactivestreams::AllowanceSemaphore initialResponseAllowance_;
};

template <class T>
void StreamSubscriptionRequesterBase<T>::onNext(Payload request) {
  switch (state_) {
    case State::NEW: {
      state_ = State::REQUESTED;
      // FIXME: find a root cause of this assymetry; the problem here is that
      // the Base::request might be delivered after the whole thing is shut
      // down, if one uses InlineConnection.
      size_t initialN = initialResponseAllowance_.drainWithLimit(
          Frame_REQUEST_N::kMaxRequestN);
      size_t remainingN = initialResponseAllowance_.drain();
      // Send as much as possible with the initial request.
      CHECK_GE(Frame_REQUEST_N::kMaxRequestN, initialN);
      auto flags = initialN > 0 ? FrameFlags_REQN_PRESENT : FrameFlags_EMPTY;
      T frame(
          streamId_,
          flags,
          static_cast<uint32_t>(initialN),
          FrameMetadata::empty(),
          std::move(request));
      // We must inform ConsumerMixin about an implicit allowance we have
      // requested from the remote end.
      addImplicitAllowance(initialN);
      connection_->onNextFrame(frame);
      // Pump the remaining allowance into the ConsumerMixin _after_ sending the
      // initial request.
      if (remainingN) {
        Base::request(remainingN);
      }
    } break;
    case State::REQUESTED:
      break;
    case State::CLOSED:
      break;
  }
}

template <class T>
void StreamSubscriptionRequesterBase<T>::request(size_t n) {
  switch (state_) {
    case State::NEW:
      // The initial request has not been sent out yet, hence we must accumulate
      // the unsynchronised allowance, portion of which will be sent out with
      // the initial request frame, and the rest will be dispatched via
      // Base:request (ultimately by sending REQUEST_N frames).
      initialResponseAllowance_.release(n);
      break;
    case State::REQUESTED:
      Base::request(n);
      break;
    case State::CLOSED:
      break;
  }
}

template <class T>
void StreamSubscriptionRequesterBase<T>::cancel() {
  switch (state_) {
    case State::NEW:
      state_ = State::CLOSED;
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
      break;
    case State::REQUESTED: {
      state_ = State::CLOSED;
      Frame_CANCEL frame(streamId_);
      connection_->onNextFrame(frame);
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
    } break;
    case State::CLOSED:
      break;
  }
}

template <class T>
void StreamSubscriptionRequesterBase<T>::endStream(
    StreamCompletionSignal signal) {
  switch (state_) {
    case State::NEW:
    case State::REQUESTED:
      // Spontaneous ::endStream signal means an error.
      DCHECK(StreamCompletionSignal::GRACEFUL != signal);
      state_ = State::CLOSED;
      break;
    case State::CLOSED:
      break;
  }
  Base::endStream(signal);
}

template <class T>
void StreamSubscriptionRequesterBase<T>::onNextFrame(Frame_RESPONSE& frame) {
  bool end = false;
  switch (state_) {
    case State::NEW:
      // Cannot receive a frame before sending the initial request.
      CHECK(false);
      break;
    case State::REQUESTED:
      if (frame.header_.flags_ & FrameFlags_COMPLETE) {
        state_ = State::CLOSED;
        end = true;
      }
      break;
    case State::CLOSED:
      break;
  }
  Base::onNextFrame(frame);
  if (end) {
    connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
  }
}

template <class T>
void StreamSubscriptionRequesterBase<T>::onNextFrame(Frame_ERROR& frame) {
  switch (state_) {
    case State::NEW:
      // Cannot receive a frame before sending the initial request.
      CHECK(false);
      break;
    case State::REQUESTED:
      state_ = State::CLOSED;
      Base::onError(
          std::runtime_error(frame.data_->moveToFbString().toStdString()));
      connection_->endStream(streamId_, StreamCompletionSignal::ERROR);
      break;
    case State::CLOSED:
      break;
  }
}
}

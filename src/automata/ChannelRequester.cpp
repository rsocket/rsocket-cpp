// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/ChannelRequester.h"

namespace reactivesocket {

void ChannelRequester::onSubscribeImpl(
    std::shared_ptr<Subscription> subscription) noexcept {
  CHECK(State::NEW == state_);
  if (ConsumerMixin::isTerminated()) {
    subscription->cancel();
    return;
  }
  publisherSubscribe(subscription);
  // Request the first payload immediately.
  subscription->request(1);
}

void ChannelRequester::onNextImpl(Payload request) noexcept {
  switch (state_) {
    case State::NEW: {
      state_ = State::REQUESTED;
      // FIXME: find a root cause of this asymmetry; the problem here is that
      // the ConsumerMixin::request might be delivered after the whole thing is
      // shut down, if one uses InlineConnection.
      size_t initialN = initialResponseAllowance_.drainWithLimit(
          Frame_REQUEST_N::kMaxRequestN);
      size_t remainingN = initialResponseAllowance_.drain();
      // Send as much as possible with the initial request.
      CHECK_GE(Frame_REQUEST_N::kMaxRequestN, initialN);
      newStream(
          StreamType::CHANNEL,
          static_cast<uint32_t>(initialN),
          std::move(request),
          false);
      // We must inform ConsumerMixin about an implicit allowance we have
      // requested from the remote end.
      ConsumerMixin::addImplicitAllowance(initialN);
      // Pump the remaining allowance into the ConsumerMixin _after_ sending the
      // initial request.
      if (remainingN) {
        ConsumerMixin::generateRequest(remainingN);
      }
    } break;
    case State::REQUESTED: {
      debugCheckOnNextOnCompleteOnError();

      // TODO(t16487710): Subsequent messages from requester to responder MUST
      // be sent as PAYLOAD frames
      writePayload(std::move(request), 0);
      break;
    }
    case State::CLOSED:
      break;
  }
}

// TODO: consolidate code in onCompleteImpl, onErrorImpl, cancelImpl
void ChannelRequester::onCompleteImpl() noexcept {
  switch (state_) {
    case State::NEW:
      state_ = State::CLOSED;
      closeStream(StreamCompletionSignal::COMPLETE);
      break;
    case State::REQUESTED: {
      state_ = State::CLOSED;
      // TODO(t16487710): Subsequent messages from requester to responder MUST
      // be sent as PAYLOAD frames
      newStream(StreamType::REQUEST_RESPONSE, 0, Payload(), true);
      closeStream(StreamCompletionSignal::COMPLETE);
    } break;
    case State::CLOSED:
      break;
  }
}

void ChannelRequester::onErrorImpl(folly::exception_wrapper ex) noexcept {
  switch (state_) {
    case State::NEW:
      state_ = State::CLOSED;
      closeStream(StreamCompletionSignal::APPLICATION_ERROR);
      break;
    case State::REQUESTED: {
      applicationError(ex.what().toStdString());
    } break;
    case State::CLOSED:
      break;
  }
}

void ChannelRequester::requestImpl(size_t n) noexcept {
  switch (state_) {
    case State::NEW:
      // The initial request has not been sent out yet, hence we must accumulate
      // the unsynchronised allowance, portion of which will be sent out with
      // the initial request frame, and the rest will be dispatched via
      // ConsumerMixin:request (ultimately by sending REQUEST_N frames).
      initialResponseAllowance_.release(n);
      break;
    case State::REQUESTED:
      ConsumerMixin::generateRequest(n);
      break;
    case State::CLOSED:
      break;
  }
}

void ChannelRequester::cancelImpl() noexcept {
  switch (state_) {
    case State::NEW:
      state_ = State::CLOSED;
      closeStream(StreamCompletionSignal::CANCEL);
      break;
    case State::REQUESTED: {
      state_ = State::CLOSED;
      cancelStream();
    } break;
    case State::CLOSED:
      break;
  }
}

void ChannelRequester::endStream(StreamCompletionSignal signal) {
  switch (state_) {
    case State::NEW:
    case State::REQUESTED:
      // Spontaneous ::endStream signal messagesns an error.
      DCHECK(StreamCompletionSignal::COMPLETE != signal);
      DCHECK(StreamCompletionSignal::CANCEL != signal);
      state_ = State::CLOSED;
      break;
    case State::CLOSED:
      break;
  }
  terminatePublisher(signal);
  ConsumerMixin::endStream(signal);
}

void ChannelRequester::onNextFrame(Frame_PAYLOAD&& frame) {
  bool end = false;
  switch (state_) {
    case State::NEW:
      // Cannot receive a frame before sending the initial request.
      CHECK(false);
      break;
    case State::REQUESTED:
      if (!!(frame.header_.flags_ & FrameFlags::COMPLETE)) {
        state_ = State::CLOSED;
        end = true;
      }
      break;
    case State::CLOSED:
      break;
  }

  processPayload(std::move(frame.payload_));

  if (end) {
    closeStream(StreamCompletionSignal::COMPLETE);
  }
}

void ChannelRequester::onNextFrame(Frame_ERROR&& frame) {
  switch (state_) {
    case State::NEW:
      // Cannot receive a frame before sending the initial request.
      CHECK(false);
      break;
    case State::REQUESTED:
      state_ = State::CLOSED;
      ConsumerMixin::onError(
          std::runtime_error(frame.payload_.moveDataToString()));
      closeStream(StreamCompletionSignal::ERROR);
      break;
    case State::CLOSED:
      break;
  }
}

void ChannelRequester::onNextFrame(Frame_REQUEST_N&& frame) {
  PublisherMixin::processRequestN(frame.requestN_);
}

} // reactivesocket

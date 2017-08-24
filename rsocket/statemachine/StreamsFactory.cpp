// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/statemachine/StreamsFactory.h"

#include "rsocket/statemachine/ChannelRequester.h"
#include "rsocket/statemachine/ChannelResponder.h"
#include "rsocket/statemachine/RSocketStateMachine.h"
#include "rsocket/statemachine/RequestResponseRequester.h"
#include "rsocket/statemachine/RequestResponseResponder.h"
#include "rsocket/statemachine/StreamRequester.h"
#include "rsocket/statemachine/StreamResponder.h"

#include "yarpl/flowable/Flowable.h"
#include "yarpl/single/Singles.h"

namespace rsocket {

using namespace yarpl;

StreamsFactory::StreamsFactory(
    RSocketStateMachine& connection,
    RSocketMode mode)
    : connection_(connection),
      nextStreamId_(
          mode == RSocketMode::CLIENT
              ? 1 /*Streams initiated by a client MUST use
                    odd-numbered stream identifiers*/
              : 2 /*streams initiated by the server MUST use
                    even-numbered stream identifiers*/) {}

void subscribeToErrorFlowable(
    Reference<yarpl::flowable::Subscriber<Payload>> responseSink) {
  yarpl::flowable::Flowables::error<Payload>(
      std::runtime_error(
          "state machine is disconnected/closed"))
      ->subscribe(std::move(responseSink));
}

void subscribeToErrorSingle(
    Reference<yarpl::single::SingleObserver<Payload>> responseSink) {
  yarpl::single::Singles::error<Payload>(
      std::runtime_error(
          "state machine is disconnected/closed"))
      ->subscribe(std::move(responseSink));
}

Reference<yarpl::flowable::Subscriber<Payload>>
StreamsFactory::createChannelRequester(
    Reference<yarpl::flowable::Subscriber<Payload>> responseSink) {
  if (connection_.isDisconnectedOrClosed()) {
    subscribeToErrorFlowable(std::move(responseSink));
    return nullptr;
  }

  ChannelRequester::Parameters params(
      connection_.shared_from_this(), getNextStreamId());
  auto stateMachine = yarpl::make_ref<ChannelRequester>(params);
  connection_.addStream(params.streamId, stateMachine);
  stateMachine->subscribe(std::move(responseSink));
  return stateMachine;
}

void StreamsFactory::createStreamRequester(
    Payload request,
    Reference<yarpl::flowable::Subscriber<Payload>> responseSink) {
  if (connection_.isDisconnectedOrClosed()) {
    subscribeToErrorFlowable(std::move(responseSink));
    return;
  }

  StreamRequester::Parameters params(
      connection_.shared_from_this(), getNextStreamId());
  auto stateMachine =
      yarpl::make_ref<StreamRequester>(params, std::move(request));
  connection_.addStream(params.streamId, stateMachine);
  stateMachine->subscribe(std::move(responseSink));
}

void StreamsFactory::createStreamRequester(
    Reference<yarpl::flowable::Subscriber<Payload>> responseSink,
    StreamId streamId,
    size_t n) {
  if (connection_.isDisconnectedOrClosed()) {
    subscribeToErrorFlowable(std::move(responseSink));
    return;
  }

  StreamRequester::Parameters params(connection_.shared_from_this(), streamId);
  auto stateMachine = yarpl::make_ref<StreamRequester>(params, Payload());
  // Set requested to true (since cold resumption)
  stateMachine->setRequested(n);
  connection_.addStream(params.streamId, stateMachine);
  stateMachine->subscribe(std::move(responseSink));
}

void StreamsFactory::createRequestResponseRequester(
    Payload payload,
    Reference<yarpl::single::SingleObserver<Payload>> responseSink) {
  if (connection_.isDisconnectedOrClosed()) {
    subscribeToErrorSingle(std::move(responseSink));
    return;
  }

  RequestResponseRequester::Parameters params(
      connection_.shared_from_this(), getNextStreamId());
  auto stateMachine =
      yarpl::make_ref<RequestResponseRequester>(params, std::move(payload));
  connection_.addStream(params.streamId, stateMachine);
  stateMachine->subscribe(std::move(responseSink));
}

StreamId StreamsFactory::getNextStreamId() {
  StreamId streamId = nextStreamId_;
  CHECK(streamId <= std::numeric_limits<int32_t>::max() - 2);
  nextStreamId_ += 2;
  return streamId;
}

void StreamsFactory::setNextStreamId(StreamId streamId) {
  nextStreamId_ = streamId + 2;
}

bool StreamsFactory::registerNewPeerStreamId(StreamId streamId) {
  DCHECK(streamId != 0);
  if (nextStreamId_ % 2 == streamId % 2) {
    // if this is an unknown stream to the socket and this socket is
    // generating
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

Reference<ChannelResponder> StreamsFactory::createChannelResponder(
    uint32_t initialRequestN,
    StreamId streamId) {
  ChannelResponder::Parameters params(connection_.shared_from_this(), streamId);
  auto stateMachine =
      yarpl::make_ref<ChannelResponder>(initialRequestN, params);
  connection_.addStream(streamId, stateMachine);
  return stateMachine;
}

Reference<yarpl::flowable::Subscriber<Payload>>
StreamsFactory::createStreamResponder(
    uint32_t initialRequestN,
    StreamId streamId) {
  StreamResponder::Parameters params(connection_.shared_from_this(), streamId);
  auto stateMachine = yarpl::make_ref<StreamResponder>(initialRequestN, params);
  connection_.addStream(streamId, stateMachine);
  return stateMachine;
}

Reference<yarpl::single::SingleObserver<Payload>>
StreamsFactory::createRequestResponseResponder(StreamId streamId) {
  RequestResponseResponder::Parameters params(
      connection_.shared_from_this(), streamId);
  auto stateMachine = yarpl::make_ref<RequestResponseResponder>(params);
  connection_.addStream(streamId, stateMachine);
  return stateMachine;
}

} // reactivesocket

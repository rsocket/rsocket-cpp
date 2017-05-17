// Copyright 2004-present Facebook. All Rights Reserved.

#include "StreamsFactory.h"
#include "src/statemachine/RSocketStateMachine.h"
#include "src/statemachine/ChannelRequester.h"
#include "src/statemachine/ChannelResponder.h"
#include "src/statemachine/RequestResponseRequester.h"
#include "src/statemachine/RequestResponseResponder.h"
#include "src/statemachine/StreamRequester.h"
#include "src/statemachine/StreamResponder.h"

namespace reactivesocket {

using namespace yarpl;

StreamsFactory::StreamsFactory(
    RSocketStateMachine& connection,
    ReactiveSocketMode mode)
    : connection_(connection),
      nextStreamId_(
          mode == ReactiveSocketMode::CLIENT
              ? 1 /*Streams initiated by a client MUST use
                    odd-numbered stream identifiers*/
              : 2 /*streams initiated by the server MUST use
                    even-numbered stream identifiers*/) {}

Reference<yarpl::flowable::Subscriber<Payload>> StreamsFactory::createChannelRequester(
    Reference<yarpl::flowable::Subscriber<Payload>> responseSink) {
  ChannelRequester::Parameters params(connection_.shared_from_this(), getNextStreamId());
  auto automaton = yarpl::make_ref<ChannelRequester>(params);
  connection_.addStream(params.streamId, automaton);
  automaton->subscribe(std::move(responseSink));
  return automaton;
}

void StreamsFactory::createStreamRequester(
    Payload request,
    StreamId streamId,
    Reference<yarpl::flowable::Subscriber<Payload>> responseSink) {
  StreamRequester::Parameters params(connection_.shared_from_this(), streamId);
  auto automaton =
      yarpl::make_ref<StreamRequester>(params, std::move(request));
  automaton->setState(StreamRequester::State::REQUESTED);
  connection_.addStream(params.streamId, automaton);
  automaton->subscribe(std::move(responseSink));
}

StreamId StreamsFactory::createStreamRequester(
    Payload request,
    yarpl::Reference<yarpl::flowable::Subscriber<Payload>> responseSink) {
  auto nextStreamId = getNextStreamId();
  StreamRequester::Parameters params(
      connection_.shared_from_this(), nextStreamId);
  auto automaton =
      yarpl::make_ref<StreamRequester>(params, std::move(request));
  connection_.addStream(params.streamId, automaton);
  automaton->subscribe(std::move(responseSink));
  return nextStreamId;
}

void StreamsFactory::createRequestResponseRequester(
    Payload payload,
    Reference<yarpl::flowable::Subscriber<Payload>> responseSink) {
  RequestResponseRequester::Parameters params(connection_.shared_from_this(), getNextStreamId());
  auto automaton =
      yarpl::make_ref<RequestResponseRequester>(params, std::move(payload));
  connection_.addStream(params.streamId, automaton);
  automaton->subscribe(std::move(responseSink));
}

StreamId StreamsFactory::getNextStreamId() {
  StreamId streamId = nextStreamId_;
  CHECK(streamId <= std::numeric_limits<int32_t>::max() - 2);
  nextStreamId_ += 2;
  return streamId;
}

void StreamsFactory::setNextStreamId(StreamId nextStreamId) {
  if (nextStreamId > nextStreamId_) {
    nextStreamId_ = nextStreamId;
  }
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
  auto automaton = yarpl::make_ref<ChannelResponder>(initialRequestN, params);
  connection_.addStream(streamId, automaton);
  return automaton;
}

Reference<yarpl::flowable::Subscriber<Payload>> StreamsFactory::createStreamResponder(
    uint32_t initialRequestN,
    StreamId streamId) {
  StreamResponder::Parameters params(connection_.shared_from_this(), streamId);
  auto automaton = yarpl::make_ref<StreamResponder>(initialRequestN, params);
  connection_.addStream(streamId, automaton);
  return automaton;
}

Reference<yarpl::flowable::Subscriber<Payload>>
StreamsFactory::createRequestResponseResponder(
    StreamId streamId) {
  RequestResponseResponder::Parameters params(connection_.shared_from_this(), streamId);
  auto automaton = yarpl::make_ref<RequestResponseResponder>(params);
  connection_.addStream(streamId, automaton);
  return automaton;
}

} // reactivesocket

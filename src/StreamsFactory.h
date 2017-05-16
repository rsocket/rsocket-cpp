// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/Common.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/flowable/Subscription.h"

namespace folly {
class Executor;
}

namespace reactivesocket {

class ConnectionAutomaton;
class ChannelResponder;
struct Payload;

class StreamsFactory {
 public:
  StreamsFactory(ConnectionAutomaton& connection, ReactiveSocketMode mode);

  yarpl::Reference<yarpl::flowable::Subscriber<Payload>> createChannelRequester(
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>> responseSink);

  void createStreamRequester(
      Payload request,
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>> responseSink);

  void createRequestResponseRequester(
      Payload payload,
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>> responseSink);

  // TODO: the return type should not be the automaton type, but something
  // generic
  yarpl::Reference<ChannelResponder> createChannelResponder(
      uint32_t initialRequestN,
      StreamId streamId);

  yarpl::Reference<yarpl::flowable::Subscriber<Payload>> createStreamResponder(
      uint32_t initialRequestN,
      StreamId streamId);

  yarpl::Reference<yarpl::flowable::Subscriber<Payload>> createRequestResponseResponder(
      StreamId streamId);

  bool registerNewPeerStreamId(StreamId streamId);
  StreamId getNextStreamId();

 private:
  ConnectionAutomaton& connection_;
  StreamId nextStreamId_;
  StreamId lastPeerStreamId_{0};
};
} // reactivesocket

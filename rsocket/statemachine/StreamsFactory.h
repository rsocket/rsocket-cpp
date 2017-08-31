// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/internal/Common.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/flowable/Subscription.h"
#include "yarpl/single/SingleObserver.h"

namespace folly {
class Executor;
}

namespace rsocket {

class RSocketStateMachine;
class ChannelResponder;
struct Payload;

class StreamsFactory {
 public:
  StreamsFactory(RSocketStateMachine& connection, RSocketMode mode);

  yarpl::Reference<yarpl::flowable::Subscriber<Payload>> createChannelRequester(
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>> responseSink);

  void createStreamRequester(
      Payload request,
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>> responseSink);

  void createStreamRequester(
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>> responseSink,
      StreamId streamId,
      size_t n);

  void createRequestResponseRequester(
      Payload payload,
      yarpl::Reference<yarpl::single::SingleObserver<Payload>> responseSink);

  // TODO: the return type should not be the stateMachine type, but something
  // generic
  yarpl::Reference<ChannelResponder> createChannelResponder(
      uint32_t initialRequestN,
      StreamId streamId);

  yarpl::Reference<yarpl::flowable::Subscriber<Payload>> createStreamResponder(
      uint32_t initialRequestN,
      StreamId streamId);

  yarpl::Reference<yarpl::single::SingleObserver<Payload>>
  createRequestResponseResponder(StreamId streamId);

  bool registerNewPeerStreamId(StreamId streamId);
  StreamId getNextStreamId();

  void setNextStreamId(StreamId streamId);

 private:
  RSocketStateMachine& connection_;
  StreamId nextStreamId_;
  StreamId lastPeerStreamId_{0};
};
} // reactivesocket

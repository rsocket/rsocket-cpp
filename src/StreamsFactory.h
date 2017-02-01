// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/Common.h"
#include "src/ReactiveStreamsCompat.h"

namespace folly {
class Executor;
}

namespace reactivesocket {

class ConnectionAutomaton;
class ChannelResponder;
struct Payload;

class StreamsFactory {
 public:
  StreamsFactory(
      std::shared_ptr<ConnectionAutomaton> connection,
      ReactiveSocketMode mode);

  std::shared_ptr<Subscriber<Payload>> createChannelRequester(
      std::shared_ptr<Subscriber<Payload>> responseSink,
      folly::Executor& executor);

  void createStreamRequester(
      Payload request,
      std::shared_ptr<Subscriber<Payload>> responseSink,
      folly::Executor& executor);

  void createSubscriptionRequester(
      Payload request,
      std::shared_ptr<Subscriber<Payload>> responseSink,
      folly::Executor& executor);

  void createRequestResponseRequester(
      Payload payload,
      std::shared_ptr<Subscriber<Payload>> responseSink,
      folly::Executor& executor);

  // TODO: the return type should not be the automaton type, but something
  // generic
  std::shared_ptr<ChannelResponder> createChannelResponder(
      uint32_t initialRequestN,
      StreamId streamId,
      folly::Executor& executor);

  std::shared_ptr<Subscriber<Payload>> createStreamResponder(
      uint32_t initialRequestN,
      StreamId streamId,
      folly::Executor& executor);

  std::shared_ptr<Subscriber<Payload>> createSubscriptionResponder(
      uint32_t initialRequestN,
      StreamId streamId,
      folly::Executor& executor);

  std::shared_ptr<Subscriber<Payload>> createRequestResponseResponder(
      StreamId streamId,
      folly::Executor& executor);

  bool registerNewPeerStreamId(StreamId streamId);
  StreamId getNextStreamId();

 private:
  std::shared_ptr<ConnectionAutomaton> connection_;
  StreamId nextStreamId_;
  StreamId lastPeerStreamId_{0};
};
} // reactivesocket

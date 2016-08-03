// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/ReactiveSocket.h"

#include <cassert>
#include <functional>
#include <memory>

#include <folly/ExceptionWrapper.h>
#include <folly/Memory.h>
#include <folly/MoveWrapper.h>

#include "src/ConnectionAutomaton.h"
#include "src/DuplexConnection.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/RequestHandler.h"
#include "src/automata/ChannelRequester.h"
#include "src/automata/ChannelResponder.h"
#include "src/automata/StreamRequester.h"
#include "src/automata/StreamResponder.h"
#include "src/automata/SubscriptionRequester.h"
#include "src/automata/SubscriptionResponder.h"

namespace reactivesocket {

ReactiveSocket::~ReactiveSocket() {
  stats_.socketClosed();
  // Force connection closure, this will trigger terminal signals to be
  // delivered to all stream automata.
  connection_->disconnect();
}

std::unique_ptr<ReactiveSocket> ReactiveSocket::fromClientConnection(
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<RequestHandler> handler,
    Stats& stats) {
  std::unique_ptr<ReactiveSocket> socket(new ReactiveSocket(
      false, std::move(connection), std::move(handler), stats));
  socket->connection_->connect(true);
  return socket;
}

std::unique_ptr<ReactiveSocket> ReactiveSocket::fromServerConnection(
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<RequestHandler> handler,
    Stats& stats) {
  std::unique_ptr<ReactiveSocket> socket(new ReactiveSocket(
      true, std::move(connection), std::move(handler), stats));
  socket->connection_->connect(false);
  return socket;
}

Subscriber<Payload>& ReactiveSocket::requestChannel(
    Subscriber<Payload>& responseSink) {
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  ChannelResponder::Parameters params = {connection_, streamId};
  auto automaton = new ChannelRequester(params);
  connection_->addStream(streamId, *automaton);
  automaton->subscribe(responseSink);
  responseSink.onSubscribe(*automaton);
  automaton->start();
  return *automaton;
}

void ReactiveSocket::requestStream(
    Payload request,
    Subscriber<Payload>& responseSink) {
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  StreamRequester::Parameters params = {connection_, streamId};
  auto automaton = new StreamRequester(params);
  connection_->addStream(streamId, *automaton);
  automaton->subscribe(responseSink);
  responseSink.onSubscribe(*automaton);
  automaton->onNext(std::move(request));
  automaton->start();
}

void ReactiveSocket::requestSubscription(
    Payload request,
    Subscriber<Payload>& responseSink) {
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  SubscriptionRequester::Parameters params = {connection_, streamId};
  auto automaton = new SubscriptionRequester(params);
  connection_->addStream(streamId, *automaton);
  automaton->subscribe(responseSink);
  responseSink.onSubscribe(*automaton);
  automaton->onNext(std::move(request));
  automaton->start();
}

void ReactiveSocket::requestFireAndForget(Payload request) {
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  Frame_REQUEST_FNF frame(
      streamId,
      FrameFlags_EMPTY,
      FrameMetadata::empty(),
      std::move(std::move(request)));
  connection_->onNextFrame(frame);
}

ReactiveSocket::ReactiveSocket(
    bool isServer,
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<RequestHandler> handler,
    Stats& stats)
    : connection_(new ConnectionAutomaton(
          std::move(connection),
          std::bind(
              &ReactiveSocket::createResponder,
              this,
              std::placeholders::_1,
              std::placeholders::_2))),
      handler_(std::move(handler)),
      nextStreamId_(isServer ? 1 : 2),
      stats_(stats) {
  stats_.socketCreated();
}

bool ReactiveSocket::createResponder(
    StreamId streamId,
    Payload& serializedFrame) {
  auto type = FrameHeader::peekType(*serializedFrame);
  switch (type) {
    case FrameType::REQUEST_CHANNEL: {
      Frame_REQUEST_CHANNEL frame;
      if (!frame.deserializeFrom(std::move(serializedFrame))) {
        return false;
      }
      ChannelResponder::Parameters params = {connection_, streamId};
      auto automaton = new ChannelResponder(params);
      connection_->addStream(streamId, *automaton);
      auto& requestSink =
          handler_->handleRequestChannel(std::move(frame.data_), *automaton);
      automaton->subscribe(requestSink);
      automaton->onNextFrame(frame);
      requestSink.onSubscribe(*automaton);
      automaton->start();
      break;
    }
    case FrameType::REQUEST_STREAM: {
      Frame_REQUEST_STREAM frame;
      if (!frame.deserializeFrom(std::move(serializedFrame))) {
        return false;
      }
      StreamResponder::Parameters params = {connection_, streamId};
      auto automaton = new StreamResponder(params);
      connection_->addStream(streamId, *automaton);
      handler_->handleRequestStream(std::move(frame.data_), *automaton);
      automaton->onNextFrame(frame);
      automaton->start();
      break;
    }
    case FrameType::REQUEST_SUB: {
      Frame_REQUEST_SUB frame;
      if (!frame.deserializeFrom(std::move(serializedFrame))) {
        return false;
      }
      SubscriptionResponder::Parameters params = {connection_, streamId};
      auto automaton = new SubscriptionResponder(params);
      connection_->addStream(streamId, *automaton);
      handler_->handleRequestSubscription(std::move(frame.data_), *automaton);
      automaton->onNextFrame(frame);
      automaton->start();
      break;
    }
    case FrameType::REQUEST_FNF: {
      Frame_REQUEST_FNF frame;
      if (!frame.deserializeFrom(std::move(serializedFrame))) {
        return false;
      }
      // no stream tracking is necessary
      handler_->handleFireAndForgetRequest(std::move(frame.data_));
      break;
    }
    // Other frames cannot start a stream.
    default:
      return false;
  }
  return true;
}
}

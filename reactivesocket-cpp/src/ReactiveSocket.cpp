// Copyright 2004-present Facebook. All Rights Reserved.

#include "reactivesocket-cpp/src/ReactiveSocket.h"

#include <cassert>
#include <functional>
#include <memory>

#include <folly/ExceptionWrapper.h>
#include <folly/Memory.h>
#include <folly/MoveWrapper.h>

#include "reactivesocket-cpp/src/DuplexConnection.h"
#include "reactivesocket-cpp/src/Frame.h"
#include "reactivesocket-cpp/src/Payload.h"
#include "reactivesocket-cpp/src/RequestHandler.h"
#include "reactivesocket-cpp/src/automata/ChannelRequester.h"
#include "reactivesocket-cpp/src/automata/ChannelResponder.h"
#include "reactivesocket-cpp/src/automata/SubscriptionRequester.h"
#include "reactivesocket-cpp/src/automata/SubscriptionResponder.h"

namespace lithium {
namespace reactivesocket {

ReactiveSocket::~ReactiveSocket() {}

std::unique_ptr<ReactiveSocket> ReactiveSocket::fromClientConnection(
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<RequestHandler> handler) {
  std::unique_ptr<ReactiveSocket> socket(
      new ReactiveSocket(false, std::move(connection), std::move(handler)));
  socket->connection_.connect();
  return socket;
}

std::unique_ptr<ReactiveSocket> ReactiveSocket::fromServerConnection(
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<RequestHandler> handler) {
  std::unique_ptr<ReactiveSocket> socket(
      new ReactiveSocket(true, std::move(connection), std::move(handler)));
  socket->connection_.connect();
  return socket;
}

Subscriber<Payload>& ReactiveSocket::requestChannel(
    Subscriber<Payload>& responseSink) {
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  ChannelResponder::Parameters params = {&connection_, streamId};
  auto automaton = new ChannelRequester(params);
  connection_.addStream(streamId, *automaton);
  automaton->subscribe(responseSink);
  responseSink.onSubscribe(*automaton);
  automaton->start();
  return *automaton;
}

void ReactiveSocket::requestSubscription(
    Payload request,
    Subscriber<Payload>& responseSink) {
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  SubscriptionRequester::Parameters params = {&connection_, streamId};
  auto automaton = new SubscriptionRequester(params);
  connection_.addStream(streamId, *automaton);
  automaton->subscribe(responseSink);
  responseSink.onSubscribe(*automaton);
  automaton->onNext(std::move(request));
  automaton->start();
}

ReactiveSocket::ReactiveSocket(
    bool isServer,
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<RequestHandler> handler)
    : handler_(std::move(handler)),
      nextStreamId_(isServer ? 1 : 2),
      connection_(
          std::move(connection),
          std::bind(
              &ReactiveSocket::createResponder,
              this,
              std::placeholders::_1,
              std::placeholders::_2)) {}

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
      ChannelResponder::Parameters params = {&connection_, streamId};
      auto automaton = new ChannelResponder(params);
      connection_.addStream(streamId, *automaton);
      auto& requestSink =
          handler_->handleRequestChannel(std::move(frame.data_), *automaton);
      automaton->subscribe(requestSink);
      automaton->onNextFrame(frame);
      requestSink.onSubscribe(*automaton);
      automaton->start();
      break;
    }
    case FrameType::REQUEST_SUB: {
      Frame_REQUEST_SUB frame;
      if (!frame.deserializeFrom(std::move(serializedFrame))) {
        return false;
      }
      SubscriptionResponder::Parameters params = {&connection_, streamId};
      auto automaton = new SubscriptionResponder(params);
      connection_.addStream(streamId, *automaton);
      handler_->handleRequestSubscription(std::move(frame.data_), *automaton);
      automaton->onNextFrame(frame);
      automaton->start();
      break;
    }
    // Other frames cannot start a stream.
    default:
      return false;
  }
  return true;
}
}
}

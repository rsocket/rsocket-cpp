// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <map>

#include <folly/SocketAddress.h>
#include "rsocket/Payload.h"
#include "rsocket/RSocket.h"
#include "rsocket/RSocketRequester.h"

#include "tck-test/BaseSubscriber.h"
#include "tck-test/TestSuite.h"

namespace folly {
class EventBase;
}

namespace rsocket {

class ReactiveSocket;

namespace tck {

class SubscribeCommand;
class RequestCommand;
class AwaitCommand;
class CancelCommand;
class AssertCommand;

class TestInterpreter {
  class TestClient {
   public:
    explicit TestClient(std::shared_ptr<RSocketClient> c)
        : client(std::move(c)) {
      auto rs = client->getRequester();
      requester = std::move(rs);
    }
    std::shared_ptr<RSocketClient> client;
    std::shared_ptr<RSocketRequester> requester;
  };

 public:
  TestInterpreter(const Test& test, folly::SocketAddress address);

  bool run();

 private:
  void handleSubscribe(const SubscribeCommand& command);
  void handleRequest(const RequestCommand& command);
  void handleAwait(const AwaitCommand& command);
  void handleCancel(const CancelCommand& command);
  void handleAssert(const AssertCommand& command);

  yarpl::Reference<BaseSubscriber> getSubscriber(const std::string& id);

  folly::SocketAddress address_;
  const Test& test_;
  std::map<std::string, std::string> interactionIdToType_;
  std::map<std::string, yarpl::Reference<BaseSubscriber>> testSubscribers_;
  std::map<std::string, std::shared_ptr<TestClient>> testClient_;
};

} // namespace tck
} // namespace rsocket

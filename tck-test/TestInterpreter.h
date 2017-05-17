// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <map>
#include "src/Payload.h"
#include "src/internal/ReactiveStreamsCompat.h"
#include "test/deprecated/ReactiveSocket.h"

#include "tck-test/TestSubscriber.h"
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
 public:
  TestInterpreter(const Test& test, ReactiveSocket& reactiveSocket);

  bool run(folly::EventBase* evb);

 private:
  void handleSubscribe(const SubscribeCommand& command);
  void handleRequest(const RequestCommand& command);
  void handleAwait(const AwaitCommand& command);
  void handleCancel(const CancelCommand& command);
  void handleAssert(const AssertCommand& command);

  yarpl::Reference<TestSubscriber> createTestSubscriber(const std::string& id);
  yarpl::Reference<TestSubscriber> getSubscriber(const std::string& id);

  ReactiveSocket* reactiveSocket_;
  const Test& test_;
  std::map<std::string, std::string> interactionIdToType_;
  std::map<std::string, yarpl::Reference<TestSubscriber>> testSubscribers_;
};

} // tck
} // reactivesocket

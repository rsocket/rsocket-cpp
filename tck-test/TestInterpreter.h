// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <map>

#include "src/Payload.h"
#include "src/RSocketRequester.h"

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
 public:
  TestInterpreter(
      const Test& test,
      RSocketRequester* requester);

  bool run();

 private:
  void handleSubscribe(const SubscribeCommand& command);
  void handleRequest(const RequestCommand& command);
  void handleAwait(const AwaitCommand& command);
  void handleCancel(const CancelCommand& command);
  void handleAssert(const AssertCommand& command);

  yarpl::Reference<BaseSubscriber> getSubscriber(const std::string& id);

  RSocketRequester* requester_;
  const Test& test_;
  std::map<std::string, std::string> interactionIdToType_;
  std::map<std::string, yarpl::Reference<BaseSubscriber>> testSubscribers_;
};

} // tck
} // reactivesocket

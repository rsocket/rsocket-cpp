// Copyright 2004-present Facebook. All Rights Reserved.

#include "tck-test/TestInterpreter.h"

#include <folly/Format.h>
#include <folly/String.h>
#include <folly/io/async/EventBase.h>

#include "tck-test/TypedCommands.h"

using namespace folly;
using namespace yarpl;

namespace rsocket {
namespace tck {

TestInterpreter::TestInterpreter(
    const Test& test,
    ReactiveSocket& reactiveSocket)
    : reactiveSocket_(&reactiveSocket), test_(test) {
  DCHECK(!test.empty());
}

bool TestInterpreter::run(folly::EventBase* evb) {
  LOG(INFO) << "Executing test: " << test_.name() << " ("
            << test_.commands().size() - 1 << " commands)";

  int i = 0;
  try {
    for (const auto& command : test_.commands()) {
      VLOG(1) << folly::sformat(
          "Executing command: [{}] {}", i, command.name());
      ++i;
      if (command.name() == "subscribe") {
        auto subscribe = command.as<SubscribeCommand>();
        evb->runInEventBaseThreadAndWait(
            [this, &subscribe]() { handleSubscribe(subscribe); });
      } else if (command.name() == "request") {
        auto request = command.as<RequestCommand>();
        evb->runInEventBaseThreadAndWait(
            [this, &request]() { handleRequest(request); });
      } else if (command.name() == "await") {
        auto await = command.as<AwaitCommand>();
        handleAwait(await);
      } else if (command.name() == "cancel") {
        auto cancel = command.as<CancelCommand>();
        evb->runInEventBaseThreadAndWait(
            [this, &cancel]() { handleCancel(cancel); });
      } else if (command.name() == "assert") {
        auto assert = command.as<AssertCommand>();
        handleAssert(assert);
      } else {
        LOG(ERROR) << "unknown command " << command.name();
        throw std::runtime_error("unknown command");
      }
    }
  } catch (const std::exception& ex) {
    LOG(ERROR) << folly::sformat(
        "Test {} failed executing command {}. {}",
        test_.name(),
        test_.commands()[i - 1].name(),
        ex.what());
    return false;
  }
  LOG(INFO) << "Test " << test_.name() << " succeeded";
  return true;
}

void TestInterpreter::handleSubscribe(const SubscribeCommand& command) {
  interactionIdToType_[command.id()] = command.type();
  if (command.isRequestResponseType()) {
    auto testSubscriber = createTestSubscriber(command.id());
    reactiveSocket_->requestResponse(
        Payload(command.payloadData(), command.payloadMetadata()),
        std::move(testSubscriber));
  } else if (command.isRequestStreamType()) {
    auto testSubscriber = createTestSubscriber(command.id());
    reactiveSocket_->requestStream(
        Payload(command.payloadData(), command.payloadMetadata()),
        std::move(testSubscriber));
  } else {
    throw std::runtime_error("unsupported interaction type");
  }
}

void TestInterpreter::handleRequest(const RequestCommand& command) {
  getSubscriber(command.id())->request(command.n());
}

void TestInterpreter::handleCancel(const CancelCommand& command) {
  getSubscriber(command.id())->cancel();
}

void TestInterpreter::handleAwait(const AwaitCommand& command) {
  if (command.isTerminalType()) {
    LOG(INFO) << "... await: terminal event";
    getSubscriber(command.id())->awaitTerminalEvent();
  } else if (command.isAtLeastType()) {
    LOG(INFO) << "... await: terminal at least " << command.numElements();
    getSubscriber(command.id())->awaitAtLeast(command.numElements());
  } else if (command.isNoEventsType()) {
    LOG(INFO) << "... await: no events for " << command.waitTime() << "ms";
    getSubscriber(command.id())->awaitNoEvents(command.waitTime());
  } else {
    throw std::runtime_error("unsupported await type");
  }
}

void TestInterpreter::handleAssert(const AssertCommand& command) {
  if (command.isNoErrorAssert()) {
    LOG(INFO) << "... assert: no error";
    getSubscriber(command.id())->assertNoErrors();
  } else if (command.isErrorAssert()) {
    LOG(INFO) << "... assert: error";
    getSubscriber(command.id())->assertError();
  } else if (command.isReceivedAssert()) {
    LOG(INFO) << "... assert: values";
    getSubscriber(command.id())->assertValues(command.values());
  } else if (command.isReceivedNAssert()) {
    LOG(INFO) << "... assert: value count " << command.valueCount();
    getSubscriber(command.id())->assertValueCount(command.valueCount());
  } else if (command.isReceivedAtLeastAssert()) {
    LOG(INFO) << "... assert: received at least " << command.valueCount();
    getSubscriber(command.id())->assertReceivedAtLeast(command.valueCount());
  } else if (command.isCompletedAssert()) {
    LOG(INFO) << "... assert: completed";
    getSubscriber(command.id())->assertCompleted();
  } else if (command.isNotCompletedAssert()) {
    LOG(INFO) << "... assert: not completed";
    getSubscriber(command.id())->assertNotCompleted();
  } else if (command.isCanceledAssert()) {
    LOG(INFO) << "... assert: canceled";
    getSubscriber(command.id())->assertCanceled();
  } else {
    throw std::runtime_error("unsupported assert type");
  }
}

yarpl::Reference<TestSubscriber> TestInterpreter::createTestSubscriber(
    const std::string& id) {
  if (testSubscribers_.find(id) != testSubscribers_.end()) {
    throw std::runtime_error("test subscriber with the same id already exists");
  }

  auto testSubscriber = make_ref<TestSubscriber>();
  testSubscribers_[id] = testSubscriber;
  return testSubscriber;
}

yarpl::Reference<TestSubscriber> TestInterpreter::getSubscriber(
    const std::string& id) {
  auto found = testSubscribers_.find(id);
  if (found == testSubscribers_.end()) {
    throw std::runtime_error("unable to find test subscriber with provided id");
  }
  return found->second;
}

} // tck
} // reactivesocket

// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rsocket/tck-test/TestInterpreter.h"

#include <folly/Format.h>
#include <folly/String.h>
#include <folly/io/async/EventBase.h>

#include "rsocket/RSocket.h"
#include "rsocket/tck-test/FlowableSubscriber.h"
#include "rsocket/tck-test/SingleSubscriber.h"
#include "rsocket/tck-test/TypedCommands.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"

using namespace folly;
using namespace yarpl;

namespace rsocket {
namespace tck {

TestInterpreter::TestInterpreter(const Test& test, SocketAddress address)
    : address_(address), test_(test) {
  DCHECK(!test.empty());
}

bool TestInterpreter::run() {
  LOG(INFO) << "Executing test: " << test_.name() << " ("
            << test_.commands().size() - 1 << " commands)";

  int i = 0;
  try {
    for (const auto& command : test_.commands()) {
      VLOG(1) << folly::sformat(
          "Executing command: [{}] {}", i, command.name());
      ++i;
      if (command.name() == "subscribe") {
        const auto subscribe = command.as<SubscribeCommand>();
        handleSubscribe(subscribe);
      } else if (command.name() == "request") {
        const auto request = command.as<RequestCommand>();
        handleRequest(request);
      } else if (command.name() == "await") {
        const auto await = command.as<AwaitCommand>();
        handleAwait(await);
      } else if (command.name() == "cancel") {
        const auto cancel = command.as<CancelCommand>();
        handleCancel(cancel);
      } else if (command.name() == "assert") {
        const auto assert = command.as<AssertCommand>();
        handleAssert(assert);
      } else if (command.name() == "disconnect") {
        const auto disconnect = command.as<DisconnectCommand>();
        handleDisconnect(disconnect);
      } else if (command.name() == "resume") {
        const auto resume = command.as<ResumeCommand>();
        handleResume(resume);
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

void TestInterpreter::handleDisconnect(const DisconnectCommand& command) {
  if (testClient_.find(command.clientId()) != testClient_.end()) {
    LOG(INFO) << "Disconnecting the client";
    testClient_[command.clientId()]->client->disconnect(
        std::runtime_error("disconnect triggered from client"));
  }
}

void TestInterpreter::handleResume(const ResumeCommand& command) {
  if (testClient_.find(command.clientId()) != testClient_.end()) {
    LOG(INFO) << "Resuming the client";
    testClient_[command.clientId()]->client->resume().get();
  }
}

void TestInterpreter::handleSubscribe(const SubscribeCommand& command) {
  // If client does not exist, create a new client.
  if (testClient_.find(command.clientId()) == testClient_.end()) {
    SetupParameters setupParameters;
    if (test_.resumption()) {
      setupParameters.resumable = true;
    }
    auto client = RSocket::createConnectedClient(
                      std::make_unique<TcpConnectionFactory>(
                          *worker_.getEventBase(), std::move(address_)),
                      std::move(setupParameters))
                      .get();
    testClient_[command.clientId()] =
        std::make_shared<TestClient>(move(client));
  }

  CHECK(
      testSubscribers_.find(command.clientId() + command.id()) ==
      testSubscribers_.end());

  if (command.isRequestResponseType()) {
    auto testSubscriber = std::make_shared<SingleSubscriber>();
    testSubscribers_[command.clientId() + command.id()] = testSubscriber;
    testClient_[command.clientId()]
        ->requester
        ->requestResponse(
            Payload(command.payloadData(), command.payloadMetadata()))
        ->subscribe(std::move(testSubscriber));
  } else if (command.isRequestStreamType()) {
    auto testSubscriber = std::make_shared<FlowableSubscriber>();
    testSubscribers_[command.clientId() + command.id()] = testSubscriber;
    testClient_[command.clientId()]
        ->requester
        ->requestStream(
            Payload(command.payloadData(), command.payloadMetadata()))
        ->subscribe(std::move(testSubscriber));
  } else {
    throw std::runtime_error("unsupported interaction type");
  }
}

void TestInterpreter::handleRequest(const RequestCommand& command) {
  getSubscriber(command.clientId() + command.id())->request(command.n());
}

void TestInterpreter::handleCancel(const CancelCommand& command) {
  getSubscriber(command.clientId() + command.id())->cancel();
}

void TestInterpreter::handleAwait(const AwaitCommand& command) {
  if (command.isTerminalType()) {
    LOG(INFO) << "... await: terminal event";
    getSubscriber(command.clientId() + command.id())->awaitTerminalEvent();
  } else if (command.isAtLeastType()) {
    LOG(INFO) << "... await: terminal at least " << command.numElements();
    getSubscriber(command.clientId() + command.id())
        ->awaitAtLeast(command.numElements());
  } else if (command.isNoEventsType()) {
    LOG(INFO) << "... await: no events for " << command.waitTime() << "ms";
    getSubscriber(command.clientId() + command.id())
        ->awaitNoEvents(command.waitTime());
  } else {
    throw std::runtime_error("unsupported await type");
  }
}

void TestInterpreter::handleAssert(const AssertCommand& command) {
  if (command.isNoErrorAssert()) {
    LOG(INFO) << "... assert: no error";
    getSubscriber(command.clientId() + command.id())->assertNoErrors();
  } else if (command.isErrorAssert()) {
    LOG(INFO) << "... assert: error";
    getSubscriber(command.clientId() + command.id())->assertError();
  } else if (command.isReceivedAssert()) {
    LOG(INFO) << "... assert: values";
    getSubscriber(command.clientId() + command.id())
        ->assertValues(command.values());
  } else if (command.isReceivedNAssert()) {
    LOG(INFO) << "... assert: value count " << command.valueCount();
    getSubscriber(command.clientId() + command.id())
        ->assertValueCount(command.valueCount());
  } else if (command.isReceivedAtLeastAssert()) {
    LOG(INFO) << "... assert: received at least " << command.valueCount();
    getSubscriber(command.clientId() + command.id())
        ->assertReceivedAtLeast(command.valueCount());
  } else if (command.isCompletedAssert()) {
    LOG(INFO) << "... assert: completed";
    getSubscriber(command.clientId() + command.id())->assertCompleted();
  } else if (command.isNotCompletedAssert()) {
    LOG(INFO) << "... assert: not completed";
    getSubscriber(command.clientId() + command.id())->assertNotCompleted();
  } else if (command.isCanceledAssert()) {
    LOG(INFO) << "... assert: canceled";
    getSubscriber(command.clientId() + command.id())->assertCanceled();
  } else {
    throw std::runtime_error("unsupported assert type");
  }
}

std::shared_ptr<BaseSubscriber> TestInterpreter::getSubscriber(
    const std::string& id) {
  const auto found = testSubscribers_.find(id);
  if (found == testSubscribers_.end()) {
    throw std::runtime_error("unable to find test subscriber with provided id");
  }
  return found->second;
}

} // namespace tck
} // namespace rsocket

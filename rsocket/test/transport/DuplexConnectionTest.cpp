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

#include "DuplexConnectionTest.h"

#include <folly/io/IOBuf.h>
#include "yarpl/test_utils/Mocks.h"

namespace rsocket {
namespace tests {

using namespace folly;
using namespace rsocket;
using namespace ::testing;

void makeMultipleSetInputGetOutputCalls(
    std::unique_ptr<DuplexConnection> serverConnection,
    EventBase* serverEvb,
    std::unique_ptr<DuplexConnection> clientConnection,
    EventBase* clientEvb) {
  auto serverSubscriber = std::make_shared<
      yarpl::mocks::MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*serverSubscriber, onSubscribe_(_));
  EXPECT_CALL(*serverSubscriber, onNext_(_)).Times(10);

  serverEvb->runInEventBaseThreadAndWait([&] {
    // Keep receiving messages from different subscribers
    serverConnection->setInput(serverSubscriber);
  });

  for (int i = 0; i < 10; ++i) {
    auto clientSubscriber = std::make_shared<
        yarpl::mocks::MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
    EXPECT_CALL(*clientSubscriber, onSubscribe_(_));
    EXPECT_CALL(*clientSubscriber, onNext_(_));

    clientEvb->runInEventBaseThreadAndWait([&] {
      // Set another subscriber and receive messages
      clientConnection->setInput(clientSubscriber);
      // Get another subscriber and send messages
      clientConnection->send(folly::IOBuf::copyBuffer("0123456"));
    });
    serverSubscriber->awaitFrames(1);

    serverEvb->runInEventBaseThreadAndWait(
        [&] { serverConnection->send(folly::IOBuf::copyBuffer("6543210")); });
    clientSubscriber->awaitFrames(1);

    clientEvb->runInEventBaseThreadAndWait(
        [subscriber = std::move(clientSubscriber)]() {
          // Enables calling setInput again with another subscriber.
          subscriber->subscription()->cancel();
        });
  }

  // Cleanup
  serverEvb->runInEventBaseThreadAndWait(
      [subscriber = std::move(serverSubscriber)] {
        subscriber->subscription()->cancel();
      });
  clientEvb->runInEventBaseThreadAndWait(
      [connection = std::move(clientConnection)] {});
  serverEvb->runInEventBaseThreadAndWait(
      [connection = std::move(serverConnection)] {});
}

/**
 * Closing an Input or Output should not effect the other.
 */
void verifyInputAndOutputIsUntied(
    std::unique_ptr<DuplexConnection> serverConnection,
    EventBase* serverEvb,
    std::unique_ptr<DuplexConnection> clientConnection,
    EventBase* clientEvb) {
  auto serverSubscriber = std::make_shared<
      yarpl::mocks::MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*serverSubscriber, onSubscribe_(_));
  EXPECT_CALL(*serverSubscriber, onNext_(_)).Times(3);

  serverEvb->runInEventBaseThreadAndWait(
      [&] { serverConnection->setInput(serverSubscriber); });

  auto clientSubscriber = std::make_shared<
      yarpl::mocks::MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*clientSubscriber, onSubscribe_(_));

  clientEvb->runInEventBaseThreadAndWait([&] {
    clientConnection->setInput(clientSubscriber);
    clientConnection->send(folly::IOBuf::copyBuffer("0123456"));
  });
  serverSubscriber->awaitFrames(1);

  clientEvb->runInEventBaseThreadAndWait([&] {
    // Close the client subscriber
    {
      clientSubscriber->subscription()->cancel();
      auto deleteSubscriber = std::move(clientSubscriber);
    }
    // Output is still active
    clientConnection->send(folly::IOBuf::copyBuffer("0123456"));
  });
  serverSubscriber->awaitFrames(1);

  // Another client subscriber
  clientSubscriber = std::make_shared<
      yarpl::mocks::MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*clientSubscriber, onSubscribe_(_));
  EXPECT_CALL(*clientSubscriber, onNext_(_));
  clientEvb->runInEventBaseThreadAndWait([&] {
    // Set new input subscriber
    clientConnection->setInput(clientSubscriber);
    clientConnection->send(folly::IOBuf::copyBuffer("0123456"));
  });
  serverSubscriber->awaitFrames(1);

  // Still sending message from server to the client.
  serverEvb->runInEventBaseThreadAndWait(
      [&] { serverConnection->send(folly::IOBuf::copyBuffer("6543210")); });
  clientSubscriber->awaitFrames(1);

  // Cleanup
  clientEvb->runInEventBaseThreadAndWait(
      [subscriber = std::move(clientSubscriber)] {
        subscriber->subscription()->cancel();
      });
  serverEvb->runInEventBaseThreadAndWait(
      [subscriber = std::move(serverSubscriber)] {
        subscriber->subscription()->cancel();
      });
  clientEvb->runInEventBaseThreadAndWait(
      [connection = std::move(clientConnection)] {});
  serverEvb->runInEventBaseThreadAndWait(
      [connection = std::move(serverConnection)] {});
}

void verifyClosingInputAndOutputDoesntCloseConnection(
    std::unique_ptr<rsocket::DuplexConnection> serverConnection,
    folly::EventBase* serverEvb,
    std::unique_ptr<rsocket::DuplexConnection> clientConnection,
    folly::EventBase* clientEvb) {
  auto serverSubscriber = std::make_shared<
      yarpl::mocks::MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*serverSubscriber, onSubscribe_(_));

  serverEvb->runInEventBaseThreadAndWait(
      [&] { serverConnection->setInput(serverSubscriber); });

  auto clientSubscriber = std::make_shared<
      yarpl::mocks::MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*clientSubscriber, onSubscribe_(_));

  clientEvb->runInEventBaseThreadAndWait(
      [&] { clientConnection->setInput(clientSubscriber); });

  // Close all subscribers
  clientEvb->runInEventBaseThreadAndWait([input = std::move(clientSubscriber)] {
    input->subscription()->cancel();
  });

  serverEvb->runInEventBaseThreadAndWait([input = std::move(serverSubscriber)] {
    input->subscription()->cancel();
  });

  // Set new subscribers as the connection is not closed
  serverSubscriber = std::make_shared<
      yarpl::mocks::MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*serverSubscriber, onSubscribe_(_));
  EXPECT_CALL(*serverSubscriber, onNext_(_)).Times(1);
  // The subscriber is to be closed, as the subscription is not cancelled
  // but the connection is closed at the end
  EXPECT_CALL(*serverSubscriber, onComplete_());

  serverEvb->runInEventBaseThreadAndWait(
      [&] { serverConnection->setInput(serverSubscriber); });

  clientSubscriber = std::make_shared<
      yarpl::mocks::MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*clientSubscriber, onSubscribe_(_));
  EXPECT_CALL(*clientSubscriber, onNext_(_)).Times(1);
  // The subscriber is to be closed, as the subscription is not cancelled
  // but the connection is closed at the end
  EXPECT_CALL(*clientSubscriber, onComplete_());

  clientEvb->runInEventBaseThreadAndWait([&] {
    clientConnection->setInput(clientSubscriber);
    clientConnection->send(folly::IOBuf::copyBuffer("0123456"));
  });
  serverSubscriber->awaitFrames(1);

  // Wait till client is ready before sending message from server.
  serverEvb->runInEventBaseThreadAndWait(
      [&] { serverConnection->send(folly::IOBuf::copyBuffer("6543210")); });
  clientSubscriber->awaitFrames(1);

  // Cleanup
  clientEvb->runInEventBaseThreadAndWait(
      [connection = std::move(clientConnection)] {});
  serverEvb->runInEventBaseThreadAndWait(
      [connection = std::move(serverConnection)] {});
}

} // namespace tests
} // namespace rsocket

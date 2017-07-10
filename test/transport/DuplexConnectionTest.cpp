#include "DuplexConnectionTest.h"

#include <folly/io/IOBuf.h>
#include "../test_utils/Mocks.h"

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
  auto serverSubscriber =
      yarpl::make_ref<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*serverSubscriber, onSubscribe_(_));
  EXPECT_CALL(*serverSubscriber, onNext_(_)).Times(10);
  yarpl::Reference<Subscriber<std::unique_ptr<folly::IOBuf>>> serverOutput;
  yarpl::Reference<MockSubscription> serverSubscription;

  serverEvb->runInEventBaseThreadAndWait(
      [&connection = serverConnection,
          &input = serverSubscriber,
          &output = serverOutput,
          &subscription = serverSubscription]() {
        // Keep receiving messages from different subscribers
        connection->setInput(input);
        // Get another subscriber and send messages
        output = connection->getOutput();
        subscription = yarpl::make_ref<MockSubscription>();
        EXPECT_CALL(*subscription, request_(_)).Times(AtLeast(1));
        EXPECT_CALL(*subscription, cancel_());
        output->onSubscribe(subscription);
      });

  for (int i = 0; i < 10; ++i) {
    auto clientSubscriber =
        yarpl::make_ref<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
    EXPECT_CALL(*clientSubscriber, onSubscribe_(_));
    EXPECT_CALL(*clientSubscriber, onNext_(_));
    yarpl::Reference<MockSubscription> clientSubscription;

    clientEvb->runInEventBaseThreadAndWait(
        [&connection = clientConnection,
            &input = clientSubscriber,
            &subscription = clientSubscription]() {
          // Set another subscriber and receive messages
          connection->setInput(input);
          // Get another subscriber and send messages
          auto output = connection->getOutput();
          subscription = yarpl::make_ref<MockSubscription>();
          EXPECT_CALL(*subscription, request_(_)).Times(AtLeast(1));
          EXPECT_CALL(*subscription, cancel_());
          output->onSubscribe(subscription);
          output->onNext(folly::IOBuf::copyBuffer("01234"));
          output->onComplete();
        });
    serverSubscriber->awaitFrames(1);

    serverEvb->runInEventBaseThreadAndWait(
        [&output = serverOutput]() {
      output->onNext(folly::IOBuf::copyBuffer("43210"));
    });
    clientSubscriber->awaitFrames(1);

    clientEvb->runInEventBaseThreadAndWait(
        [subscriber = std::move(clientSubscriber),
            subscription = std::move(clientSubscription)]() {
          subscription->cancel();
          // Enables calling setInput again with another subscriber.
          subscriber->subscription()->cancel();
        });
  }

  // Cleanup
  serverEvb->runInEventBaseThreadAndWait(
      [subscriber = std::move(serverSubscriber),
          output = std::move(serverOutput),
          subscription = std::move(serverSubscription)]() {
        output->onComplete();
        subscription->cancel();
        subscriber->subscription()->cancel();
      });
  clientEvb->runInEventBaseThreadAndWait([& connection = clientConnection]() {
    auto connectionDeleter = std::move(connection);
  });
  serverEvb->runInEventBaseThreadAndWait([& connection = serverConnection]() {
    auto connectionDeleter = std::move(connection);
  });
}

/**
 * Closing an Input or Output should not effect the other.
 */
void verifyInputAndOutputIsUntied(
    std::unique_ptr<DuplexConnection> serverConnection,
    EventBase* serverEvb,
    std::unique_ptr<DuplexConnection> clientConnection,
    EventBase* clientEvb) {
  auto serverSubscriber =
      yarpl::make_ref<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*serverSubscriber, onSubscribe_(_));
  EXPECT_CALL(*serverSubscriber, onNext_(_)).Times(3);
  yarpl::Reference<Subscriber<std::unique_ptr<folly::IOBuf>>> serverOutput;
  auto serverSubscription = yarpl::make_ref<MockSubscription>();
  EXPECT_CALL(*serverSubscription, request_(_)).Times(AtLeast(1));
  EXPECT_CALL(*serverSubscription, cancel_());

  serverEvb->runInEventBaseThreadAndWait(
      [&connection = serverConnection,
          &input = serverSubscriber,
          &output = serverOutput,
          &subscription = serverSubscription]() {
        connection->setInput(input);
        output = connection->getOutput();
        output->onSubscribe(subscription);
      });

  auto clientSubscriber =
      yarpl::make_ref<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*clientSubscriber, onSubscribe_(_));
  yarpl::Reference<Subscriber<std::unique_ptr<folly::IOBuf>>> clientOutput;
  auto clientSubscription = yarpl::make_ref<MockSubscription>();
  EXPECT_CALL(*clientSubscription, request_(_)).Times(AtLeast(1));
  EXPECT_CALL(*clientSubscription, cancel_());

  clientEvb->runInEventBaseThreadAndWait(
      [&connection = clientConnection,
          &input = clientSubscriber,
          &output = clientOutput,
          &subscription = clientSubscription]() {
        connection->setInput(input);
        output = connection->getOutput();
        output->onSubscribe(subscription);
        output->onNext(folly::IOBuf::copyBuffer("01234"));
      });
  serverSubscriber->awaitFrames(1);

  clientEvb->runInEventBaseThreadAndWait(
      [&subscriber = clientSubscriber, &output = clientOutput]() {
        // Close the client subscriber
        {
          subscriber->subscription()->cancel();
          auto deleteSubscriber = std::move(subscriber);
        }
        // Output is still active
        output->onNext(folly::IOBuf::copyBuffer("01234"));
      });
  serverSubscriber->awaitFrames(1);

  // Another client subscriber
  clientSubscriber =
      yarpl::make_ref<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*clientSubscriber, onSubscribe_(_));
  EXPECT_CALL(*clientSubscriber, onNext_(_));
  clientEvb->runInEventBaseThreadAndWait(
      [&connection = clientConnection,
          &input = clientSubscriber,
          &output = clientOutput]() {
        // Set new input subscriber
        connection->setInput(input);
        output->onNext(folly::IOBuf::copyBuffer("01234"));
      });
  serverSubscriber->awaitFrames(1);

  // Close output subscriber of client
  clientEvb->runInEventBaseThreadAndWait(
      [output = std::move(clientOutput),
          subscription = std::move(clientSubscription)]() {
        subscription->cancel();
        output->onComplete();
      });

  // Still sending message from server to the client.
  serverEvb->runInEventBaseThreadAndWait([&output = serverOutput]() {
        output->onNext(folly::IOBuf::copyBuffer("43210"));
        output->onComplete();
      });
  clientSubscriber->awaitFrames(1);

  // Cleanup
  clientEvb->runInEventBaseThreadAndWait(
      [subscriber = std::move(clientSubscriber)]() {
        subscriber->subscription()->cancel();
      });
  serverEvb->runInEventBaseThreadAndWait(
      [subscriber = std::move(serverSubscriber),
          output = std::move(serverOutput),
          subscription = std::move(serverSubscription)]() {
        subscription->cancel();
        output->onComplete();
        subscriber->subscription()->cancel();
      });
  clientEvb->runInEventBaseThreadAndWait([& connection = clientConnection]() {
    auto connectionDeleter = std::move(connection);
  });
  serverEvb->runInEventBaseThreadAndWait([& connection = serverConnection]() {
    auto connectionDeleter = std::move(connection);
  });
}

void verifyClosingInputAndOutputDoesntCloseConnection(
    std::unique_ptr<rsocket::DuplexConnection> serverConnection,
    folly::EventBase* serverEvb,
    std::unique_ptr<rsocket::DuplexConnection> clientConnection,
    folly::EventBase* clientEvb) {
  auto serverSubscriber =
      yarpl::make_ref<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*serverSubscriber, onSubscribe_(_));
  yarpl::Reference<Subscriber<std::unique_ptr<folly::IOBuf>>> serverOutput;
  auto serverSubscription = yarpl::make_ref<MockSubscription>();
  EXPECT_CALL(*serverSubscription, request_(_));
  EXPECT_CALL(*serverSubscription, cancel_());

  serverEvb->runInEventBaseThreadAndWait(
      [&connection = serverConnection,
          &input = serverSubscriber,
          &output = serverOutput,
          &subscription = serverSubscription]() {
        connection->setInput(input);
        output = connection->getOutput();
        output->onSubscribe(subscription);
      });

  auto clientSubscriber =
      yarpl::make_ref<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*clientSubscriber, onSubscribe_(_));
  yarpl::Reference<Subscriber<std::unique_ptr<folly::IOBuf>>> clientOutput;
  auto clientSubscription = yarpl::make_ref<MockSubscription>();
  EXPECT_CALL(*clientSubscription, request_(_));
  EXPECT_CALL(*clientSubscription, cancel_());

  clientEvb->runInEventBaseThreadAndWait(
      [&connection = clientConnection,
          &input = clientSubscriber,
          &output = clientOutput,
          &subscription = clientSubscription]() {
        connection->setInput(input);
        output = connection->getOutput();
        output->onSubscribe(subscription);
      });

  // Close all subscribers
  clientEvb->runInEventBaseThreadAndWait(
      [input = std::move(clientSubscriber),
          output = std::move(clientOutput),
          subscription = std::move(clientSubscription)]() {
        subscription->cancel();
        output->onComplete();
        input->subscription()->cancel();
      });

  serverEvb->runInEventBaseThreadAndWait(
      [input = std::move(serverSubscriber),
          output = std::move(serverOutput),
          subscription = std::move(serverSubscription)]() {
        subscription->cancel();
        output->onComplete();
        input->subscription()->cancel();
      });

  // Set new subscribers as the connection is not closed
  serverSubscriber =
      yarpl::make_ref<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*serverSubscriber, onSubscribe_(_));
  EXPECT_CALL(*serverSubscriber, onNext_(_)).Times(1);
  // The subscriber is to be closed, as the subscription is not cancelled
  // but the connection is closed at the end
  EXPECT_CALL(*serverSubscriber, onComplete_());

  serverSubscription = yarpl::make_ref<MockSubscription>();
  EXPECT_CALL(*serverSubscription, request_(_));
  EXPECT_CALL(*serverSubscription, cancel_());

  serverEvb->runInEventBaseThreadAndWait(
      [&connection = serverConnection,
          &input = serverSubscriber,
          &output = serverOutput,
          &subscription = serverSubscription]() {
        connection->setInput(input);
        output = connection->getOutput();
        output->onSubscribe(subscription);
      });

  clientSubscriber =
      yarpl::make_ref<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*clientSubscriber, onSubscribe_(_));
  EXPECT_CALL(*clientSubscriber, onNext_(_)).Times(1);
  // The subscriber is to be closed, as the subscription is not cancelled
  // but the connection is closed at the end
  EXPECT_CALL(*clientSubscriber, onComplete_());

  clientSubscription = yarpl::make_ref<MockSubscription>();
  EXPECT_CALL(*clientSubscription, request_(_));
  EXPECT_CALL(*clientSubscription, cancel_());

  clientEvb->runInEventBaseThreadAndWait(
      [&connection = clientConnection,
          &input = clientSubscriber,
          &output = clientOutput,
          &subscription = clientSubscription]() {
        connection->setInput(input);
        output = connection->getOutput();
        output->onSubscribe(subscription);
        output->onNext(folly::IOBuf::copyBuffer("01234"));
      });
  serverSubscriber->awaitFrames(1);

  // Wait till client is ready before sending message from server.
  serverEvb->runInEventBaseThreadAndWait(
      [&output = serverOutput]() {
        output->onNext(folly::IOBuf::copyBuffer("43210"));
      });
  clientSubscriber->awaitFrames(1);

  // Cleanup
  clientEvb->runInEventBaseThreadAndWait([& connection = clientConnection]() {
    auto connectionDeleter = std::move(connection);
  });
  serverEvb->runInEventBaseThreadAndWait([& connection = serverConnection]() {
    auto connectionDeleter = std::move(connection);
  });
}

} // namespace tests
} // namespace rsocket

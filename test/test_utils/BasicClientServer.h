// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Memory.h>
#include <folly/futures/Future.h>
#include <folly/futures/SharedPromise.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <folly/io/async/EventBaseManager.h>
#include <future>
#include "src/framing/FramedDuplexConnection.h"
#include "src/RSocketParameters.h"
#include "test/test_utils/Mocks.h"

using namespace folly;
using namespace ::testing;

namespace rsocket {

class ConnectedClient {
 public:
  ConnectedClient(std::unique_ptr<DuplexConnection> connection, EventBase& evb)
      : connection_(std::move(connection)), evb_(evb) {
    VLOG(3) << "Client has connected to the server";
  }

  ~ConnectedClient() {
    evb_.add([connection = std::move(connection_)]() {
      VLOG(3) << "ConnectedClient releases connection in the event base";
    });
  }

  Future<Unit> async(Function<void(DuplexConnection*)> f) {
    Promise<Unit> promise;
    Future<Unit> result = promise.getFuture();
    evb_.runInEventBaseThread(
        [ promise = std::move(promise), f = std::move(f), this ]() mutable {
          f(connection_.get());
          promise.setValue();
        });
    return result;
  }

  void sendSetupFrame() {
    async([](DuplexConnection* connection) {
      SetupParameters setupParams;
      Frame_SETUP setupFrame(
          setupParams.resumable ? FrameFlags::RESUME_ENABLE : FrameFlags::EMPTY,
          FrameSerializer::getCurrentProtocolVersion().major,
          FrameSerializer::getCurrentProtocolVersion().minor,
          Frame_SETUP::kMaxLifetime,
          Frame_SETUP::kMaxLifetime,
          std::move(setupParams.token),
          std::move(setupParams.metadataMimeType),
          std::move(setupParams.dataMimeType),
          std::move(setupParams.payload));

      auto frameSerializer = FrameSerializer::createCurrentVersion();

      auto output = connection->getOutput();
      auto subscription = yarpl::make_ref<MockSubscription>();
      EXPECT_CALL(*subscription, cancel_());
      EXPECT_CALL(*subscription, request_(std::numeric_limits<int32_t>::max()));
      output->onSubscribe(subscription);

      VLOG(3) << "Sending SetupFrame: " << setupFrame;
      output->onNext(frameSerializer->serializeOut(std::move(setupFrame)));

      output->onComplete();
      subscription->cancel();
    }).wait();
  }

private:
  std::unique_ptr<DuplexConnection> connection_;
  EventBase& evb_;
};

class TestSubscriber : public MockSubscriber<std::unique_ptr<folly::IOBuf>> {
public:
  TestSubscriber(int requestCount = 100) {
    this->requestCount = requestCount;
  }

  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription> subscription) override {
    MockSubscriber::onSubscribe(subscription);
    subscription->request(requestCount);
  }

  void onComplete() override {
    MockSubscriber::onComplete();
    VLOG(3) << "COMPLETED";
  }

protected:
  int requestCount;
};

class ConnectedServer {
 public:
  ConnectedServer(
      std::unique_ptr<DuplexConnection> connection,
      EventBase& eventBase)
      : connection_(std::move(connection)), evb_(eventBase) {}

  ~ConnectedServer() {
    evb_.add([connection = std::move(connection_)]() {
      VLOG(3) << "ConnectedServer releases connection in the event base";
    });
  }

  void receiveSetupFrame() {
    // Receive the protocol version
    async([](DuplexConnection* connection) {
      auto serverSubscriber = yarpl::make_ref<TestSubscriber>(1);
      EXPECT_CALL(*serverSubscriber, onSubscribe_(_));
      EXPECT_CALL(*serverSubscriber, onNext_(_)).Times(AtLeast(1));
      EXPECT_CALL(*serverSubscriber, onComplete_());
      connection->setInput(serverSubscriber);
    }).wait();
    async([](DuplexConnection* connection) {
      connection->setInput(nullptr);
    }).wait();
  }

  Future<Unit> async(Function<void(DuplexConnection*)> f) {
    Promise<Unit> promise;
    Future<Unit> result = promise.getFuture();
    evb_.runInEventBaseThread(
        [ promise = std::move(promise), f = std::move(f), this ]() mutable {
          f(connection_.get());
          promise.setValue();
        });
    return result;
  }

 private:
  std::unique_ptr<DuplexConnection> connection_;
  EventBase& evb_;
};

template <typename ConnectionFactory>
class Client {
 public:
  Client(uint16_t port)
      : connectionFactory_(std::make_unique<ConnectionFactory>(
            SocketAddress("localhost", port, true))) {}

  auto connect() {
    Promise<std::unique_ptr<ConnectedClient>> promise;
    auto future = promise.getFuture();
    connectionFactory_->connect(
        [&promise](
            std::unique_ptr<DuplexConnection> connection, EventBase& evb) {
          VLOG(3) << "Connected!";

          promise.setValue(
              std::make_unique<ConnectedClient>(std::move(connection), evb));
        });

    auto&& waitedFuture = future.wait();
    if (waitedFuture.getTry().hasException()) {
      try {
        waitedFuture.getTry().exception().throw_exception();
      } catch (const std::exception& e) {
        VLOG(3) << "Client exception: " << e.what();
      }
    }
    CHECK(!waitedFuture.getTry().hasException());

    return waitedFuture.get();
  }

 private:
  std::unique_ptr<ConnectionFactory> connectionFactory_;
};

template <typename ConnectionAcceptor>
class Server {
 public:
  Server(typename ConnectionAcceptor::Options options)
      : connectionAcceptor_(options), evb_() {
    thread_ =
        std::thread([& eventBase = this->evb_]() { eventBase.loopForever(); });
    start();
  }

  ~Server() {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  std::unique_ptr<ConnectedServer> getConnected() {
    waiting_.timed_wait(std::chrono::seconds(1));
    waiting_.reset();
    CHECK(connected_) << "No client connection.";
    connected_ = false;
    return std::move(connectedServer_);
  }

  void stop() {
    evb_.terminateLoopSoon();
  }

 private:
  void start() {
    evb_.runInEventBaseThreadAndWait([this]() {
      auto waitedFuture =
          connectionAcceptor_
              .start([this](
                         std::unique_ptr<DuplexConnection> connection,
                         folly::EventBase& eventBase) {
                acceptConnection(std::move(connection), eventBase);
              })
              .wait();

      if (waitedFuture.getTry().hasException()) {
        try {
          waitedFuture.getTry().exception().throw_exception();
        } catch (const std::exception& e) {
          VLOG(3) << "Server exception: " << e.what();
        }
      }
      CHECK(!waitedFuture.getTry().hasException());
    });
  }

  void acceptConnection(
      std::unique_ptr<DuplexConnection> connection,
      folly::EventBase& eventBase) {
    VLOG(3) << "Server::acceptConnection";
    if (connected_) {
      // Only let a new connection to happen if the previous one's
      // ownership is taken by calling getConnected() by the user code
      CHECK(!connected_) << "Dropped connected client";
      return;
    }

    connectedServer_ =
        std::make_unique<ConnectedServer>(std::move(connection), eventBase);
    connected_ = true;
    waiting_.post();
  }

 private:
  ConnectionAcceptor connectionAcceptor_;
  Baton<> waiting_;
  bool connected_{};
  std::unique_ptr<ConnectedServer> connectedServer_;
  EventBase evb_;
  std::thread thread_;
};

} // namespace rsocket

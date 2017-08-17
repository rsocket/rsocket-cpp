// Copyright 2004-present Facebook. All Rights Reserved.

#include "benchmarks/Throughput.h"

#include <folly/Baton.h>
#include <folly/Benchmark.h>
#include <folly/Synchronized.h>
#include <folly/init/Init.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"
#include "yarpl/Flowable.h"

using namespace rsocket;

constexpr size_t kMessageLen = 32;

DEFINE_int32(items, 1000000, "number of items in stream");

namespace {

/// State shared across the client and server DirectDuplexConnections.
struct State {
  /// Whether one of the two connections has been destroyed.
  folly::Synchronized<bool> destroyed;
};

/// DuplexConnection that talks to another DuplexConnection via memory.
class DirectDuplexConnection : public DuplexConnection {
 public:
  DirectDuplexConnection(std::shared_ptr<State> state, folly::EventBase& evb)
      : state_{std::move(state)}, evb_{evb} {}

  ~DirectDuplexConnection() {
    *state_->destroyed.wlock() = true;
  }

  // Tie two DirectDuplexConnections together so they can talk to each other.
  void tie(DirectDuplexConnection* other) {
    other_ = other;
    other_->other_ = this;
  }

  void setInput(yarpl::Reference<DuplexConnection::Subscriber> input) override {
    input_ = std::move(input);
  }

  yarpl::Reference<DuplexConnection::Subscriber> getOutput() override {
    return yarpl::flowable::Subscribers::create<std::unique_ptr<folly::IOBuf>>(
        [this](std::unique_ptr<folly::IOBuf> buf) {
          auto destroyed = state_->destroyed.rlock();
          if (*destroyed) {
            return;
          }

          other_->evb_.runInEventBaseThread(
              [ state = state_, other = other_, b = std::move(buf) ]() mutable {
                auto destroyed = state->destroyed.rlock();
                if (*destroyed) {
                  return;
                }

                other->input_->onNext(std::move(b));
              });
        });
  }

 private:
  std::shared_ptr<State> state_;
  folly::EventBase& evb_;

  DirectDuplexConnection* other_{nullptr};

  yarpl::Reference<DuplexConnection::Subscriber> input_;
};

class Acceptor : public ConnectionAcceptor {
 public:
  explicit Acceptor(std::shared_ptr<State> state) : state_{std::move(state)} {}

  void setClientConnection(DirectDuplexConnection* connection) {
    client_ = connection;
  }

  void start(OnDuplexConnectionAccept onAccept) override {
    worker_.getEventBase()->runInEventBaseThread(
        [ this, onAccept = std::move(onAccept) ]() mutable {
          auto server = std::make_unique<DirectDuplexConnection>(
              std::move(state_), *worker_.getEventBase());
          server->tie(client_);
          onAccept(std::move(server), *worker_.getEventBase());
        });
  }

  void stop() override {}

  folly::Optional<uint16_t> listeningPort() const override {
    return folly::none;
  }

 private:
  std::shared_ptr<State> state_;

  DirectDuplexConnection* client_{nullptr};

  folly::ScopedEventBaseThread worker_;
};

class Factory : public ConnectionFactory {
 public:
  Factory() {
    auto state = std::make_shared<State>();

    connection_ = std::make_unique<DirectDuplexConnection>(
        state, *worker_.getEventBase());

    auto acceptor = std::make_unique<Acceptor>(state);
    acceptor_ = acceptor.get();

    acceptor_->setClientConnection(connection_.get());

    auto responder =
        std::make_shared<FixedResponder>(std::string(kMessageLen, 'a'));

    server_ = std::make_unique<RSocketServer>(std::move(acceptor));
    server_->start([responder](const SetupParameters&) { return responder; });
  }

  folly::Future<ConnectedDuplexConnection> connect() override {
    return folly::via(worker_.getEventBase(), [this] {
      return ConnectedDuplexConnection{std::move(connection_),
                                       *worker_.getEventBase()};
    });
  }

 private:
  std::unique_ptr<DirectDuplexConnection> connection_;

  std::unique_ptr<rsocket::RSocketServer> server_;
  Acceptor* acceptor_{nullptr};

  folly::ScopedEventBaseThread worker_;
};

std::shared_ptr<RSocketClient> makeClient() {
  auto factory = std::make_unique<Factory>();
  return RSocket::createConnectedClient(std::move(factory)).get();
}
}

BENCHMARK(StreamThroughput, n) {
  std::shared_ptr<RSocketClient> client;
  yarpl::Reference<BoundedSubscriber> subscriber;

  folly::ScopedEventBaseThread worker;

  BENCHMARK_SUSPEND {
    LOG(INFO) << "  Running with " << FLAGS_items << " items";

    client = makeClient();
    subscriber = yarpl::make_ref<BoundedSubscriber>(FLAGS_items);
  }

  client->getRequester()
      ->requestStream(Payload("InMemoryStream"))
      ->subscribe(subscriber);

  subscriber->awaitTerminalEvent();
}

// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/init/Init.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>
#include <iostream>

#include "examples/util/ExampleSubscriber.h"
#include "rsocket/RSocket.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"
#include "yarpl/Flowable.h"

using namespace ::folly;
using namespace ::rsocket_example;
using namespace ::rsocket;
using namespace yarpl::flowable;

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

namespace {
class ChannelConnectionEvents : public RSocketConnectionEvents {
 public:
  void onConnected() override {
    LOG(INFO) << "onConnected";
  }

  void onDisconnected(const folly::exception_wrapper& ex) override {
    LOG(INFO) << "onDiconnected ex=" << ex.what();
  }

  void onClosed(const folly::exception_wrapper& ex) override {
    LOG(INFO) << "onClosed ex=" << ex.what();
    closed_ = true;
  }

  bool isClosed() const {
    return closed_;
  }

 private:
  std::atomic<bool> closed_{false};
};
}

void sendRequest(std::string mimeType) {
  folly::ScopedEventBaseThread worker;
  folly::SocketAddress address;
  address.setFromHostPort(FLAGS_host, FLAGS_port);
  auto connectionEvents = std::make_shared<ChannelConnectionEvents>();
  auto client = RSocket::createConnectedClient(
                    std::make_unique<TcpConnectionFactory>(
                        *worker.getEventBase(), std::move(address)),
                    SetupParameters(mimeType, mimeType),
                    std::make_shared<RSocketResponder>(),
                    kDefaultKeepaliveInterval,
                    RSocketStats::noop(),
                    connectionEvents)
                    .get();

  std::atomic<int> rcvdCount{0};

  client->getRequester()
      ->requestStream(Payload("Bob"))
      ->take(5)
      ->subscribe([&rcvdCount](Payload p) {
        std::cout << "Received: " << p.moveDataToString() << std::endl;
        rcvdCount++;
      });

  while (rcvdCount < 5 && !connectionEvents->isClosed()) {
    std::this_thread::yield();
  }
}

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  sendRequest("application/json");
  sendRequest("text/plain");
  sendRequest("garbage");

  return 0;
}

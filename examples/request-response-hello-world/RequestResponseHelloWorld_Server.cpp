// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>
#include <thread>

#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"
#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"
#include "yarpl/Single.h"

using namespace rsocket;
using namespace yarpl;
using namespace yarpl::single;

DEFINE_int32(port, 9898, "port to connect to");

class HelloRequestResponseRequestResponder : public rsocket::RSocketResponder {
 public:
  Reference<Single<Payload>> handleRequestResponse(Payload request, StreamId)
      override {
    std::cout << "HelloRequestResponseRequestResponder.handleRequestResponse "
              << request << std::endl;

    // string from payload data
    auto requestString = request.moveDataToString();

    return Single<Payload>::create([name = std::move(requestString)](
        auto subscriber) {

      std::stringstream ss;
      ss << "Hello " << name << "!";
      std::string s = ss.str();
      subscriber->onSubscribe(SingleSubscriptions::empty());
      subscriber->onSuccess(Payload(s, "metadata"));
    });
  }
};

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  TcpConnectionAcceptor::Options opts;
  opts.port = FLAGS_port;
  opts.threads = 2;

  // RSocket server accepting on TCP
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  // global request responder
  auto responder = std::make_shared<HelloRequestResponseRequestResponder>();

  auto rawRs = rs.get();
  auto serverThread = std::thread([=] {
    // start accepting connections
    rawRs->startAndPark([responder](auto& setup) { setup.createRSocket(responder); });
  });

  // Wait for a newline on the console to terminate the server.
  std::getchar();

  rs->unpark();
  serverThread.join();

  return 0;
}

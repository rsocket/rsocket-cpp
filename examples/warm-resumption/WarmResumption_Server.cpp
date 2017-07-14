// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>
#include <thread>

#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>

#include "src/RSocket.h"
#include "src/transports/tcp/TcpConnectionAcceptor.h"
#include "yarpl/Flowable.h"

using namespace rsocket;
using namespace yarpl::flowable;

DEFINE_int32(port, 9898, "Port to accept connections on");
DEFINE_bool(resumable, true, "Whether the server supports resumption");

class HelloStreamRequestResponder : public rsocket::RSocketResponder {
 public:
  /// Handles a new inbound Stream requested by the other end.
  yarpl::Reference<Flowable<rsocket::Payload>> handleRequestStream(
      rsocket::Payload request,
      rsocket::StreamId) override {
    // string from payload data
    auto requestString = request.moveDataToString();

    return Flowables::range(1, 1000)->map([name = std::move(requestString)](
        int64_t v) {
      std::stringstream ss;
      ss << "Hello " << name << " " << v << "!";
      std::string s = ss.str();
      return Payload(s, "metadata");
    });
  }
};

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  TcpConnectionAcceptor::Options opts;
  opts.port = FLAGS_port;
  opts.threads = 1;

  // RSocket server accepting on TCP
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  // global request responder
  auto responder = std::make_shared<HelloStreamRequestResponder>();

  auto rawRs = rs.get();
  auto serverThread = std::thread([=] {
    // start accepting connections
    rawRs->startAndPark([responder](auto& setup) {
      setup.createRSocket(responder, ServerOptions(FLAGS_resumable));
    });
  });

  // Wait for a newline on the console to terminate the server.
  std::getchar();

  rs->unpark();
  serverThread.join();

  return 0;
}

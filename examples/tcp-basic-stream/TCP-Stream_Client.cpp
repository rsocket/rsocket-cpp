// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/io/async/ScopedEventBaseThread.h>
#include <iostream>
#include "examples/util/ExampleSubscriber.h"
#include "experimental/include-rsocket/RSocket.h"
#include "experimental/rsocket-src/TcpClientConnectionFactory.h"

using namespace ::reactivesocket;
using namespace ::folly;
using namespace ::rsocket_example;
using namespace ::rsocket;

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  ScopedEventBaseThread eventBaseThread;
  auto rsf = RSocket::createClientFactory(
      TcpClientConnectionFactory::create(FLAGS_host, FLAGS_port));
  rsf->connect(eventBaseThread)
      .then([](std::shared_ptr<StandardReactiveSocket> rs) {
        rs->requestStream(
            Payload("args-here"), std::make_shared<ExampleSubscriber>(5, 6));
      });

  // TODO the following should work, but has eventbase issues (rs should use the
  // correct one)
  //  auto rs = rsf->connect(eventBaseThread).get();
  //  rs->requestStream(
  //      Payload("args-here"), std::make_shared<ExampleSubscriber>(5, 6));

  std::string name;
  std::getline(std::cin, name);

  return 0;
}

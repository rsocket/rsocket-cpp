// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>

#include <folly/init/Init.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>

#include "examples/util/ExampleSubscriber.h"
#include "rsocket/RSocket.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"

#include "yarpl/Single.h"

using namespace rsocket_example;
using namespace rsocket;
using namespace yarpl;
using namespace yarpl::single;

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  folly::SocketAddress address;
  address.setFromHostPort(FLAGS_host, FLAGS_port);

  // create a client which can then make connections below
  auto rsf = RSocket::createClient(
      std::make_unique<TcpConnectionFactory>(std::move(address)));

  // connect and wait for connection
  auto rs = rsf->connect().get();

  // perform request on connected RSocket
  rs->fireAndForget(Payload("Hello World!"))->subscribe([] {
    std::cout << "wrote to network" << std::endl;
  });

  // Wait for a newline on the console to terminate the client.
  std::getchar();

  return 0;
}

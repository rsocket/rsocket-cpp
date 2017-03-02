// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/io/async/ScopedEventBaseThread.h>
#include <iostream>
#include "examples/util/ExampleSubscriber.h"
#include "rsocket/RSocket.h"
#include "rsocket/transports/TcpConnectionFactory.h"

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

  auto rsf = RSocket::createClient(
      TcpConnectionFactory::create(FLAGS_host, FLAGS_port));
  auto rs = rsf->connect().get();

  LOG(INFO) << "------------------ Hello Bob!";
  auto s1 = std::make_shared<ExampleSubscriber>(5, 6);
  rs->requestStream(Payload("Bob"), s1);
  s1->awaitTerminalEvent();

  LOG(INFO) << "------------------ Hello Jane!";
  auto s2 = std::make_shared<ExampleSubscriber>(5, 6);
  rs->requestStream(Payload("Jane"), s2);
  s2->awaitTerminalEvent();

  // TODO on shutdown the destruction of
  // ScopedEventBaseThread spits out a stacktrace
  return 0;
}

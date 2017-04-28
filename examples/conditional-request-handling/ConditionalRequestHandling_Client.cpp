// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/init/Init.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>
#include <iostream>

#include "examples/util/ExampleSubscriber.h"
#include "rsocket/RSocket.h"
#include "rsocket/transports/TcpConnectionFactory.h"
#include "yarpl/Flowable.h"
#include "yarpl/Subscriber.h"

using namespace ::reactivesocket;
using namespace ::folly;
using namespace ::rsocket_example;
using namespace ::rsocket;
using namespace yarpl;

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  auto rsf = RSocket::createClient(
      TcpConnectionFactory::create(FLAGS_host, FLAGS_port));
  auto rs = rsf->connect().get();

  LOG(INFO) << "------------------ Hello Bob!";
  auto s1 = yarpl::Reference<ExampleSubscriber>(new ExampleSubscriber(5, 6));
  rs->requestStream(Payload("Bob"))
      ->take(5)
      ->subscribe(yarpl::Reference<yarpl::Subscriber<Payload>>(s1.get()));
  s1->awaitTerminalEvent();

  LOG(INFO) << "------------------ Hello Jane!";
  auto s2 = yarpl::Reference<ExampleSubscriber>(new ExampleSubscriber(5, 6));
  rs->requestStream(Payload("Jane"))
      ->take(3)
      ->subscribe(yarpl::Reference<yarpl::Subscriber<Payload>>(s2.get()));
  s2->awaitTerminalEvent();

  // TODO on shutdown the destruction of
  // ScopedEventBaseThread spits out a stacktrace
  return 0;
}

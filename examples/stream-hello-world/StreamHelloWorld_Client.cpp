// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>

#include <folly/init/Init.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>

#include "examples/util/ExampleSubscriber.h"
#include "rsocket/RSocket.h"
#include "rsocket/transports/TcpConnectionFactory.h"

#include "yarpl/Flowable.h"
#include "yarpl/flowable/Subscriber.h"

using namespace reactivesocket;
using namespace rsocket_example;
using namespace rsocket;

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

  {
    // this example runs inside the Future.then lambda
    LOG(INFO) << "------------------ Run in future.then";
    auto s = yarpl::Reference<ExampleSubscriber>(new ExampleSubscriber(5, 6));
    rsf->connect().then([s](std::shared_ptr<RSocketRequester> rs) {
      rs->requestStream(Payload("Bob"))
          ->subscribe(
              yarpl::Reference<yarpl::flowable::Subscriber<Payload>>(s.get()));
    });
    s->awaitTerminalEvent();
  }

  {
    // this example extracts from the Future.get and runs in the main thread
    LOG(INFO) << "------------------ Run after future.get";
    auto s = yarpl::Reference<ExampleSubscriber>(new ExampleSubscriber(5, 6));
    auto rs = rsf->connect().get();
    rs->requestStream(Payload("Jane"))
        ->subscribe(
            yarpl::Reference<yarpl::flowable::Subscriber<Payload>>(s.get()));
    s->awaitTerminalEvent();
  }
  LOG(INFO) << "------------- main() terminating -----------------";

  // TODO on shutdown the destruction of
  // ScopedEventBaseThread spits out a stacktrace
  return 0;
}

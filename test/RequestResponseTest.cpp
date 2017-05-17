// Copyright 2004-present Facebook. All Rights Reserved.

#include <chrono>
#include <thread>

#include "RSocketTests.h"
#include "yarpl/Single.h"

using namespace yarpl;
using namespace yarpl::single;
using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;
using namespace std::chrono_literals;

namespace {
class TestHandler : public rsocket::RSocketResponder {
 public:
  Reference<Single<Payload>> handleRequestResponse(
      Payload request,
      StreamId streamId) override {
    std::cout << "HelloRequestResponseRequestHandler.handleRequestResponse "
              << request << std::endl;

    // string from payload data
    auto requestString = request.moveDataToString();

    return Single<Payload>::create([name = std::move(requestString)](
        auto subscriber) {

      std::stringstream ss;
      ss << "Hello " << name << "!";
      std::string s = ss.str();
      subscriber->onSuccess(Payload(s, "metadata"));
    });
  }
};
}

TEST(RequestResponseTest, StartAndShutdown) {
  auto port = randPort();
  auto server = makeServer(port, std::make_shared<TestHandler>());
  auto client = makeClient(port);
  auto requester = client->connect().get();
  requester->requestResponse(Payload("Jane"))->subscribeBlocking([](Payload p) {
    std::cout << "Received >> " << p.moveDataToString() << std::endl;
  });
}
// Copyright 2004-present Facebook. All Rights Reserved.

#include <chrono>
#include <thread>

#include "RSocketTests.h"
#include "yarpl/Single.h"
#include "yarpl/single/SingleTestObserver.h"

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

TEST(RequestResponseTest, Hello) {
  auto port = randPort();
  auto server = makeServer(port, std::make_shared<TestHandler>());
  auto client = makeClient(port);
  auto requester = client->connect().get();

  auto to = SingleTestObserver<std::string>::create();
  requester->requestResponse(Payload("Jane"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(to);
  to->awaitTerminalEvent();
  to->assertOnSuccessValue("Hello Jane!");
}
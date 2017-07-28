// Copyright 2004-present Facebook. All Rights Reserved.

#include <thread>

#include "RSocketTests.h"

#include "rsocket/RSocketServiceHandler.h"
#include "yarpl/flowable/TestSubscriber.h"

using namespace rsocket;
using namespace rsocket::tests::client_server;
using namespace yarpl::flowable;

namespace {

class HelloServiceHandler : public RSocketServiceHandler {
 public:
  folly::Expected<RSocketConnectionParams, RSocketException> onNewSetup(
      const SetupParameters&) override {
    return RSocketConnectionParams(
        std::make_shared<rsocket::tests::HelloStreamRequestHandler>());
  }

  void onNewRSocketState(
      std::shared_ptr<RSocketServerState> state,
      ResumeIdentificationToken token) override {
    store_.lock()->insert({token, std::move(state)});
  }

  folly::Expected<std::shared_ptr<RSocketServerState>, RSocketException>
  onResume(ResumeIdentificationToken token) override {
    auto itr = store_->find(token);
    CHECK(itr != store_->end());
    return itr->second;
  };

 private:
  folly::Synchronized<
      std::map<ResumeIdentificationToken, std::shared_ptr<RSocketServerState>>,
      std::mutex>
      store_;
};

} // anonymous namespace

TEST(WarmResumptionTest, SimpleStream) {
  auto server = makeResumableServer(std::make_shared<HelloServiceHandler>());
  auto client = makeResumableClient(*server->listeningPort());
  auto requester = client->getRequester();
  auto ts = TestSubscriber<std::string>::create(7 /* initialRequestN */);
  requester->requestStream(Payload("Bob"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);
  // Wait for a few frames before disconnecting.
  while (ts->getValueCount() < 3) {
    std::this_thread::yield();
  }
  client->disconnect(std::runtime_error("Test triggered disconnect"));
  EXPECT_NO_THROW(client->resume().get());
  ts->request(3);
  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(10);
}

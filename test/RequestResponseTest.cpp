// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Baton.h>
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
class TestHandlerHello : public rsocket::RSocketResponder {
 public:
  Reference<Single<Payload>> handleRequestResponse(
      Payload request,
      StreamId streamId) override {
    auto requestString = request.moveDataToString();
    return Single<Payload>::create([name = std::move(requestString)](
        auto subscriber) {
      subscriber->onSubscribe(SingleSubscriptions::empty());
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
  auto server = makeServer(port, std::make_shared<TestHandlerHello>());
  auto client = makeClient(port);
  auto requester = client->connect().get();

  auto to = SingleTestObserver<std::string>::create();
  requester->requestResponse(Payload("Jane"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(to);
  to->awaitTerminalEvent();
  to->assertOnSuccessValue("Hello Jane!");
}

namespace {
class TestHandlerCancel : public rsocket::RSocketResponder {
 public:
  TestHandlerCancel(std::shared_ptr<folly::Baton<>> onCancel)
      : onCancel_(std::move(onCancel)) {}
  Reference<Single<Payload>> handleRequestResponse(
      Payload request,
      StreamId streamId) override {
    // used to block this responder thread until a cancel is sent from client
    // over network
    auto cancelFromClient = std::make_shared<folly::Baton<>>();
    // used to signal to the client once we receive a cancel
    auto onCancel = onCancel_;
    auto requestString = request.moveDataToString();
    return Single<Payload>::create(
        [ name = std::move(requestString), cancelFromClient, onCancel ](
            auto subscriber) mutable {
          std::thread([
            subscriber = std::move(subscriber),
            name = std::move(name),
            cancelFromClient,
            onCancel
          ]() {
            auto subscription = SingleSubscriptions::create([cancelFromClient] {
              LOG(INFO) << "PRODUCER=> cancel received in callback!";
              cancelFromClient->post();
            });

            LOG(INFO) << "PRODUCER=> before onSubscribe with subscription";

            subscriber->onSubscribe(subscription);

            LOG(INFO) << "PRODUCER=> after onSubscribe with subscription";

            // simulate slow processing or IO being done
            // and block this current background thread
            // until we are cancelled
            LOG(INFO) << "PRODUCER=> Wait for cancel from client";
            cancelFromClient->wait();
            LOG(INFO) << "PRODUCER=> AFTER Wait for cancel from client";
            if (subscription->isCancelled()) {
              //  this is used by the unit test to assert the cancel was
              //  received
              LOG(INFO) << "PRODUCER=> SEND onCancel back to unit test";
              onCancel->post();
            } else {
              // if not cancelled would do work and emit here
            }
            LOG(INFO) << "PRODUCER=> background thread completing";
          }).detach();
          LOG(INFO) << "PRODUCER=> ............. after thread ...............";
        });
  }

 private:
  std::shared_ptr<folly::Baton<>> onCancel_;
};
}

TEST(RequestResponseTest, Cancel) {
  auto port = randPort();
  auto onCancel = std::make_shared<folly::Baton<>>();
  auto server = makeServer(port, std::make_shared<TestHandlerCancel>(onCancel));
  auto client = makeClient(port);
  auto requester = client->connect().get();

  auto to = SingleTestObserver<std::string>::create();
  requester->requestResponse(Payload("Jane"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(to);
  LOG(INFO) << "CONSUMER => after subscribe";
  // NOTE: if this sleep doesn't exist then the cancellation will all happen
  // locally
  // TODO instead of sleep, await Baton on responder side that signals when
  // request received, then cancel
  std::this_thread::sleep_for(500ms);
  to->cancel();
  LOG(INFO) << "CONSUMER => await cancel from test thread";
  // wait for cancel to propagate
  onCancel->wait();
  // assert no signals received on client
  to->assertNoTerminalEvent();
}

// TODO failure on responder, requester sees
// TODO failure on request, requester sees
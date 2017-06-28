// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Baton.h>
#include <thread>

#include "RSocketTests.h"
#include "yarpl/Single.h"
#include "yarpl/single/SingleTestObserver.h"

using namespace yarpl;
using namespace yarpl::single;
using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

namespace {
class TestHandlerHello : public rsocket::RSocketResponder {
 public:
  Reference<Single<Payload>> handleRequestResponse(Payload request, StreamId)
      override {
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
  auto server = makeServer(std::make_shared<TestHandlerHello>());
  auto client = makeClient(*server->listeningPort());
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
  TestHandlerCancel(
      std::shared_ptr<folly::Baton<>> onCancel,
      std::shared_ptr<folly::Baton<>> onSubscribe)
      : onCancel_(std::move(onCancel)), onSubscribe_(std::move(onSubscribe)) {}
  Reference<Single<Payload>> handleRequestResponse(Payload request, StreamId)
      override {
    // used to signal to the client when the subscribe is received
    onSubscribe_->post();
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
            auto subscription = SingleSubscriptions::create(
                [cancelFromClient] { cancelFromClient->post(); });
            subscriber->onSubscribe(subscription);
            // simulate slow processing or IO being done
            // and block this current background thread
            // until we are cancelled
            cancelFromClient->wait();
            if (subscription->isCancelled()) {
              //  this is used by the unit test to assert the cancel was
              //  received
              onCancel->post();
            } else {
              // if not cancelled would do work and emit here
            }
          }).detach();
        });
  }

 private:
  std::shared_ptr<folly::Baton<>> onCancel_;
  std::shared_ptr<folly::Baton<>> onSubscribe_;
};
}

TEST(RequestResponseTest, Cancel) {
  auto onCancel = std::make_shared<folly::Baton<>>();
  auto onSubscribe = std::make_shared<folly::Baton<>>();
  auto server =
      makeServer(std::make_shared<TestHandlerCancel>(onCancel, onSubscribe));
  auto client = makeClient(*server->listeningPort());
  auto requester = client->connect().get();

  auto to = SingleTestObserver<std::string>::create();
  requester->requestResponse(Payload("Jane"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(to);
  // NOTE: wait for server to receive request/subscribe
  // otherwise the cancellation will all happen locally
  onSubscribe->wait();
  // now cancel the local subscription
  to->cancel();
  // wait for cancel to propagate to server
  onCancel->wait();
  // assert no signals received on client
  to->assertNoTerminalEvent();
}

// TODO failure on responder, requester sees
// TODO failure on request, requester sees

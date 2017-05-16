// Copyright 2004-present Facebook. All Rights Reserved.

#include <chrono>
#include <condition_variable>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <folly/io/async/ScopedEventBaseThread.h>

#include "test/integration/ClientUtils.h"
#include "test/integration/ServerFixture.h"

using namespace std::chrono_literals;
using namespace ::reactivesocket;
using namespace ::testing;
using namespace yarpl;

using folly::ScopedEventBaseThread;

// A very simple test which tests a basic warm resumption workflow.
// This setup can be used to test varying scenarious in warm resumption.
TEST_F(ServerFixture, DISABLED_BasicWarmResumption) {
  ScopedEventBaseThread eventBaseThread;
  auto clientEvb = eventBaseThread.getEventBase();
  tests::MyConnectCallback connectCb;
  auto token = ResumeIdentificationToken::generateNew();
  std::unique_ptr<ReactiveSocket> rsocket;
  auto mySub = make_ref<tests::MySubscriber>();
  Sequence s;
  SCOPE_EXIT {
    clientEvb->runInEventBaseThreadAndWait([&]() { rsocket.reset(); });
  };

  // The thread running this test, and the thread running the clientEvb
  // have to synchronized with a barrier.
  std::mutex cvM;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lk(cvM);

  // Get a few subscriptions (happens right after connecting)
  EXPECT_CALL(*mySub, onSubscribe_()).WillOnce(Invoke([&]() {
    mySub->request(3);
  }));
  EXPECT_CALL(*mySub, onNext_("1"));
  EXPECT_CALL(*mySub, onNext_("2"));
  EXPECT_CALL(*mySub, onNext_("3")).WillOnce(Invoke([&](std::string) {
    cv.notify_all();
  }));

  // Create a RSocket and RequestStream
  clientEvb->runInEventBaseThreadAndWait([&]() {
    rsocket = tests::getRSocket(clientEvb);
    rsocket->requestStream(Payload("from client"), mySub);
    rsocket->clientConnect(
        tests::getFrameTransport(clientEvb, &connectCb, serverListenPort_),
        tests::getSetupPayload(token));
  });

  // Wait for few packets to exchange before disconnecting OR error out.
  EXPECT_EQ(
      std::cv_status::no_timeout,
      cv.wait_until(lk, std::chrono::system_clock::now() + 1000ms));

  // Disconnect
  clientEvb->runInEventBaseThreadAndWait([&]() { rsocket->disconnect(); });

  // This request should be buffered
  mySub->request(2);

  // Get subscriptions for buffered request (happens right after
  // reconnecting)
  EXPECT_CALL(*mySub, onNext_("4"));
  EXPECT_CALL(*mySub, onNext_("5")).WillOnce(Invoke([&](std::string) {
    cv.notify_all();
  }));

  // Reconnect
  clientEvb->runInEventBaseThreadAndWait([&]() {
    rsocket->tryClientResume(
        token,
        tests::getFrameTransport(clientEvb, &connectCb, serverListenPort_),
        std::make_unique<tests::ResumeCallback>());
  });

  // Wait for the remaining frames to make it OR error out.
  EXPECT_EQ(
      std::cv_status::no_timeout,
      cv.wait_until(lk, std::chrono::system_clock::now() + 1000ms));
}
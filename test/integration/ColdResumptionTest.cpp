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

// A very simple test which tests a basic cold resumption workflow.
// This setup can be used to test varying scenarious in cold resumption.
TEST_F(ServerFixture, ColdResumptionBasic) {
  ScopedEventBaseThread eventBaseThread;
  auto clientEvb = eventBaseThread.getEventBase();
  tests::MyConnectCallback connectCb;
  auto token = ResumeIdentificationToken::generateNew();
  std::unique_ptr<ReactiveSocket> rsocket;
  auto mySub1 = make_ref<tests::MySubscriber>();
  auto resumeCache = std::make_shared<ResumeCache>();
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
  EXPECT_CALL(*mySub1, onSubscribe_()).WillOnce(Invoke([&]() {
    mySub1->request(3);
  }));
  EXPECT_CALL(*mySub1, onNext_("1"));
  EXPECT_CALL(*mySub1, onNext_("2"));
  EXPECT_CALL(*mySub1, onNext_("3")).WillOnce(Invoke([&](std::string) {
    cv.notify_all();
  }));

  // Create a RSocket and RequestStream
  clientEvb->runInEventBaseThreadAndWait([&]() {
    rsocket = tests::getRSocket(clientEvb, resumeCache);
    rsocket->requestStream(Payload(""), mySub1, "query1");
    rsocket->clientConnect(
        tests::getFrameTransport(clientEvb, &connectCb, serverListenPort_),
        tests::getSetupPayload(token));
  });

  // Wait for few packets to exchange before disconnecting OR error out.
  EXPECT_EQ(std::cv_status::no_timeout, cv.wait_for(lk, 1s));

  // Disconnect. Simulate a cold restart
  clientEvb->runInEventBaseThreadAndWait([&]() { rsocket.reset(); });
  auto mySub2 = make_ref<tests::MySubscriber>();
  auto requestHandler = std::make_unique<tests::ClientRequestHandler>();

  // Get resume callbacks
  EXPECT_CALL(*requestHandler, handleResumeStream_("query1"))
      .WillOnce(Invoke([&](std::string) { return mySub2; }));

  // Get subscriptions for buffered request (happens right after
  // reconnecting)
  EXPECT_CALL(*mySub2, onSubscribe_()).WillOnce(Invoke([&]() {
    mySub2->request(2);
  }));
  EXPECT_CALL(*mySub2, onNext_("4"));
  EXPECT_CALL(*mySub2, onNext_("5")).WillOnce(Invoke([&](std::string) {
    cv.notify_all();
  }));

  // try resume
  clientEvb->runInEventBaseThreadAndWait([&]() {
    rsocket =
        tests::getRSocket(clientEvb, resumeCache, std::move(requestHandler));
    rsocket->tryClientResume(
        token,
        tests::getFrameTransport(clientEvb, &connectCb, serverListenPort_),
        std::make_unique<tests::ResumeCallback>());
  });

  // Wait for the remaining frames to make it OR error out.
  EXPECT_EQ(std::cv_status::no_timeout, cv.wait_for(lk, 1s));
}


// Test cold resumption with two active streams
TEST_F(ServerFixture, ColdResumption2Streams) {
  ScopedEventBaseThread eventBaseThread;
  auto clientEvb = eventBaseThread.getEventBase();
  tests::MyConnectCallback connectCb;
  auto token = ResumeIdentificationToken::generateNew();
  std::unique_ptr<ReactiveSocket> rsocket;
  auto mySub1 = make_ref<tests::MySubscriber>();
  auto mySub2 = make_ref<tests::MySubscriber>();
  auto resumeCache = std::make_shared<ResumeCache>();
  Sequence s;
  SCOPE_EXIT {
    clientEvb->runInEventBaseThreadAndWait([&]() { rsocket.reset(); });
  };

  // The thread running this test, and the thread running the clientEvb
  // have to synchronized with a barrier.
  std::mutex cvM;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lk(cvM);

  bool mySub1P1 = false;
  bool mySub2P1 = false;

  // Get a few subscriptions (happens right after connecting)
  EXPECT_CALL(*mySub1, onSubscribe_()).WillOnce(Invoke([&]() {
    mySub1->request(3);
  }));
  EXPECT_CALL(*mySub1, onNext_("1"));
  EXPECT_CALL(*mySub1, onNext_("2"));
  EXPECT_CALL(*mySub1, onNext_("3")).WillOnce(Invoke([&](std::string) {
    mySub1P1 = true;
    cv.notify_all();
  }));

  EXPECT_CALL(*mySub2, onSubscribe_()).WillOnce(Invoke([&]() {
    mySub2->request(2);
  }));
  EXPECT_CALL(*mySub2, onNext_("1"));
  EXPECT_CALL(*mySub2, onNext_("2")).WillOnce(Invoke([&](std::string) {
    mySub2P1 = true;
    cv.notify_all();
  }));

  // Create a RSocket and request two streams 
  clientEvb->runInEventBaseThreadAndWait([&]() {
    rsocket = tests::getRSocket(clientEvb, resumeCache);
    rsocket->requestStream(Payload(""), mySub1, "query1");
    rsocket->requestStream(Payload(""), mySub2, "query2");
    rsocket->clientConnect(
        tests::getFrameTransport(clientEvb, &connectCb, serverListenPort_),
        tests::getSetupPayload(token));
  });

  // Wait for few packets to exchange before disconnecting OR error out.
  EXPECT_TRUE(cv.wait_for(lk, 1s, [&] { return mySub1P1 && mySub2P1; }));

  // Disconnect. Simulate a cold restart
  clientEvb->runInEventBaseThreadAndWait([&]() { rsocket.reset(); });
  auto requestHandler = std::make_unique<tests::ClientRequestHandler>();
  mySub1 = make_ref<tests::MySubscriber>();
  mySub2 = make_ref<tests::MySubscriber>();
  bool mySub1P2 = false;
  bool mySub2P2 = false;

  // Get resume callbacks
  EXPECT_CALL(*requestHandler, handleResumeStream_("query1"))
      .WillOnce(Invoke([&](std::string) { return mySub1; }));
  EXPECT_CALL(*requestHandler, handleResumeStream_("query2"))
      .WillOnce(Invoke([&](std::string) { return mySub2; }));

  // Get subscriptions for buffered request (happens right after
  // reconnecting)
  EXPECT_CALL(*mySub1, onSubscribe_()).WillOnce(Invoke([&]() {
    mySub1->request(3);
  }));
  EXPECT_CALL(*mySub1, onNext_("4"));
  EXPECT_CALL(*mySub1, onNext_("5"));
  EXPECT_CALL(*mySub1, onNext_("6")).WillOnce(Invoke([&](std::string) {
    mySub1P2 = true;
    cv.notify_all();
  }));

  EXPECT_CALL(*mySub2, onSubscribe_()).WillOnce(Invoke([&]() {
    mySub2->request(2);
  }));
  EXPECT_CALL(*mySub2, onNext_("3"));
  EXPECT_CALL(*mySub2, onNext_("4")).WillOnce(Invoke([&](std::string) {
    mySub2P2 = true;
    cv.notify_all();
  }));

  // try resume
  clientEvb->runInEventBaseThreadAndWait([&]() {
    rsocket =
        tests::getRSocket(clientEvb, resumeCache, std::move(requestHandler));
    rsocket->tryClientResume(
        token,
        tests::getFrameTransport(clientEvb, &connectCb, serverListenPort_),
        std::make_unique<tests::ResumeCallback>());
  });

  // Wait for the remaining frames to make it OR error out.
  EXPECT_TRUE(cv.wait_for(lk, 1s, [&] { return mySub1P2 && mySub2P2; }));
}

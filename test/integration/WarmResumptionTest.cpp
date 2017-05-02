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

using folly::ScopedEventBaseThread;

namespace {
class MySubscriber : public Subscriber<Payload> {
 public:
  void onSubscribe(std::shared_ptr<Subscription> sub) noexcept override {
    subscription_ = sub;
    subscription_->request(1);
  }

  MOCK_METHOD1(onNext_, void(std::string));

  void onNext(Payload element) noexcept override {
    VLOG(1) << "Receiving " << element;
    onNext_(element.moveDataToString());
  }

  void onComplete() noexcept override {}

  void onError(folly::exception_wrapper ex) noexcept override {}

  // methods for testing
  void request(size_t n) {
    subscription_->request(n);
  }

 private:
  std::shared_ptr<Subscription> subscription_;
};

} // anonymous namespace

// A very simple test which tests a basic warm resumption workflow.
// This setup can be used to test varying scenarious in warm resumption.
TEST_F(ServerFixture, BasicWarmResumption) {
  ScopedEventBaseThread eventBaseThread;
  auto clientEvb = eventBaseThread.getEventBase();
  tests::MyConnectCallback connectCb;
  auto token = ResumeIdentificationToken::generateNew();
  std::unique_ptr<ReactiveSocket> rsocket;
  auto mySub = std::make_shared<MySubscriber>();
  Sequence s;
  SCOPE_EXIT {
    clientEvb->runInEventBaseThreadAndWait([&]() { rsocket.reset(); });
  };

  // The thread running this test, and the thread running the clientEvb
  // have to synchronized with a barrier.
  std::mutex cvM;
  std::condition_variable cv;
  std::unique_lock<std::mutex> lk(cvM);

  // Create a RSocket and RequestStream
  clientEvb->runInEventBaseThreadAndWait([&]() {
    rsocket = tests::getRSocket(clientEvb);
    rsocket->requestStream(Payload("from client"), mySub);
    rsocket->clientConnect(
        tests::getFrameTransport(clientEvb, &connectCb, serverListenPort_),
        tests::getSetupPayload(token));
  });

  // Get a few subscriptions
  EXPECT_CALL(*mySub, onNext_("1"));
  mySub->request(2);
  EXPECT_CALL(*mySub, onNext_("2"));
  EXPECT_CALL(*mySub, onNext_("3")).WillOnce(Invoke([&](std::string) {
    cv.notify_all();
  }));

  // Wait for few packets to exchange before disconnecting OR error out.
  EXPECT_EQ(
      std::cv_status::no_timeout,
      cv.wait_until(lk, std::chrono::system_clock::now() + 1000ms));

  // Disconnect
  clientEvb->runInEventBaseThreadAndWait([&]() { rsocket->disconnect(); });

  // This request should be buffered
  mySub->request(2);

  // Reconnect
  clientEvb->runInEventBaseThreadAndWait([&]() {
    rsocket->tryClientResume(
        token,
        tests::getFrameTransport(clientEvb, &connectCb, serverListenPort_),
        std::make_unique<tests::ResumeCallback>());
  });

  // We should get subscriptions for buffered requests
  EXPECT_CALL(*mySub, onNext_("4"));
  EXPECT_CALL(*mySub, onNext_("5")).WillOnce(Invoke([&](std::string) {
    cv.notify_all();
  }));

  // Wait for the remaining frames to make it OR error out.
  EXPECT_EQ(
      std::cv_status::no_timeout,
      cv.wait_until(lk, std::chrono::system_clock::now() + 1000ms));
}
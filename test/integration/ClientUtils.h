// Copyright 2004-present Facebook. All Rights Reserved.

#include <gmock/gmock.h>

#include <folly/io/async/AsyncSocket.h>

#include "src/internal/ClientResumeStatusCallback.h"
#include "src/framing/FrameTransport.h"
#include "src/temporary_home/NullRequestHandler.h"
#include "src/temporary_home/ReactiveSocket.h"
#include "src/internal/FollyKeepaliveTimer.h"
#include "src/framing/FramedDuplexConnection.h"
#include "src/transports/tcp/TcpDuplexConnection.h"

namespace reactivesocket {
namespace tests {

class MyConnectCallback : public folly::AsyncSocket::ConnectCallback {
 public:
  virtual ~MyConnectCallback() = default;

  void connectSuccess() noexcept override {}

  void connectErr(const folly::AsyncSocketException& ex) noexcept override {
    LOG(INFO) << "Connect Error" << ex.what();
  }
};

class ResumeCallback : public ClientResumeStatusCallback {
  void onResumeOk() noexcept override {
    LOG(INFO) << "Resumption Succeeded";
  }

  void onResumeError(folly::exception_wrapper ex) noexcept override {
    LOG(INFO) << "Resumption Error: " << ex.what();
  }

  void onConnectionError(folly::exception_wrapper ex) noexcept override {
    LOG(INFO) << "Resumption Connection Error: " << ex.what();
  }
};

class ClientRequestHandler : public DefaultRequestHandler {
 public:
  void onSubscriptionPaused(
      const yarpl::Reference<yarpl::flowable::Subscription>& subscription) noexcept override {
    LOG(INFO) << "Subscription Paused";
  }

  void onSubscriptionResumed(
      const yarpl::Reference<yarpl::flowable::Subscription>& subscription) noexcept override {
    LOG(INFO) << "Subscription Resumed";
  }

  void onSubscriberPaused(const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
                              subscriber) noexcept override {
    LOG(INFO) << "Subscriber Paused";
  }

  void onSubscriberResumed(const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
                               subscriber) noexcept override {
    LOG(INFO) << "Subscriber Resumed";
  }

  std::shared_ptr<Subscriber<Payload>> handleResumeStream(
      std::string streamName) noexcept override {
    LOG(INFO) << "Stream Resumed - " << streamName;
    return handleResumeStream_(streamName);
  }
  MOCK_METHOD1(
      handleResumeStream_,
      std::shared_ptr<Subscriber<Payload>>(std::string));
};

class MySubscriber : public yarpl::flowable::Subscriber<Payload> {
 public:

  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription> sub) noexcept override {
    subscription_ = sub;
    onSubscribe_();
  }

  void onNext(Payload element) noexcept override {
    VLOG(1) << "Receiving " << element;
    onNext_(element.moveDataToString());
  }

  MOCK_METHOD0(onSubscribe_, void());
  MOCK_METHOD1(onNext_, void(std::string));

  void onComplete() noexcept override {
    LOG(INFO) << "MySub: Received onComplete";
  }

  void onError(std::exception_ptr ex) noexcept override {
    LOG(INFO) << "MySub: Received onError";
  }

  // methods for testing
  void request(int64_t n) {
    subscription_->request(n);
  }

 private:
  yarpl::Reference<yarpl::flowable::Subscription> subscription_;
};

// Utility function to create a FrameTransport.
std::shared_ptr<FrameTransport> getFrameTransport(
    folly::EventBase* eventBase,
    folly::AsyncSocket::ConnectCallback* connectCb,
    uint32_t port);

// Utility function to create a ReactiveSocket.
std::unique_ptr<ReactiveSocket> getRSocket(
    folly::EventBase* eventBase,
    std::shared_ptr<ResumeCache> resumeCache = std::make_shared<ResumeCache>(),
    std::unique_ptr<RequestHandler> requestHandler =
        std::make_unique<ClientRequestHandler>());

// Utility function to create a SetupPayload
ConnectionSetupPayload getSetupPayload(ResumeIdentificationToken token);

}
}

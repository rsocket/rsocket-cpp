// Copyright 2004-present Facebook. All Rights Reserved.

#include <gmock/gmock.h>

#include <folly/io/async/AsyncSocket.h>

#include "src/framing/FrameTransport.h"
#include "src/framing/FramedDuplexConnection.h"
#include "src/internal/ClientResumeStatusCallback.h"
#include "src/internal/FollyKeepaliveTimer.h"
#include "src/temporary_home/NullRequestHandler.h"
#include "src/transports/tcp/TcpDuplexConnection.h"
#include "test/deprecated/ReactiveSocket.h"

namespace rsocket {
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
      const yarpl::Reference<yarpl::flowable::Subscription>&
          subscription) noexcept override {
    LOG(INFO) << "Subscription Paused";
  }

  void onSubscriptionResumed(
      const yarpl::Reference<yarpl::flowable::Subscription>&
          subscription) noexcept override {
    LOG(INFO) << "Subscription Resumed";
  }

  void onSubscriberPaused(
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          subscriber) noexcept override {
    LOG(INFO) << "Subscriber Paused";
  }

  void onSubscriberResumed(
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          subscriber) noexcept override {
    LOG(INFO) << "Subscriber Resumed";
  }
};

class MySubscriber : public yarpl::flowable::Subscriber<Payload> {
 public:
  void onSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> sub) noexcept override {
    subscription_ = sub;
    onSubscribe_();
  }

  void onNext(Payload element) noexcept override {
    VLOG(1) << "Receiving " << element;
    onNext_(element.moveDataToString());
  }

  MOCK_METHOD0(onSubscribe_, void());
  MOCK_METHOD1(onNext_, void(std::string));

  void onComplete() noexcept override {}

  void onError(std::exception_ptr ex) noexcept override {}

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
    uint32_t port) {
  folly::SocketAddress addr;
  addr.setFromLocalPort(folly::to<uint16_t>(port));
  folly::AsyncSocket::UniquePtr socket(new folly::AsyncSocket(eventBase));
  socket->connect(connectCb, addr);
  LOG(INFO) << "Attempting connection to " << addr.describe();
  std::unique_ptr<DuplexConnection> connection =
      std::make_unique<TcpDuplexConnection>(
          std::move(socket), inlineExecutor(), RSocketStats::noop());
  std::unique_ptr<DuplexConnection> framedConnection =
      std::make_unique<FramedDuplexConnection>(
          std::move(connection), *eventBase);
  return std::make_shared<FrameTransport>(std::move(framedConnection));
}

// Utility function to create a ReactiveSocket.
std::unique_ptr<ReactiveSocket> getRSocket(folly::EventBase* eventBase) {
  std::unique_ptr<ReactiveSocket> rsocket;
  std::unique_ptr<RequestHandler> requestHandler =
      std::make_unique<ClientRequestHandler>();
  rsocket = ReactiveSocket::disconnectedClient(
      *eventBase,
      std::move(requestHandler),
      RSocketStats::noop(),
      std::make_unique<FollyKeepaliveTimer>(
          *eventBase, std::chrono::seconds(10)));
  rsocket->onConnected([]() { LOG(INFO) << "ClientSocket connected"; });
  rsocket->onDisconnected([](const folly::exception_wrapper& ex) {
    LOG(INFO) << "ClientSocket disconnected: " << ex.what();
  });
  rsocket->onClosed([](const folly::exception_wrapper& ex) {
    LOG(INFO) << "ClientSocket closed: " << ex.what();
  });
  return rsocket;
}

// Utility function to create a SetupPayload
SetupParameters getSetupPayload(ResumeIdentificationToken token) {
  return SetupParameters(
      "text/plain", "text/plain", Payload("meta", "data"), true, token);
}
}
}

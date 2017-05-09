// Copyright 2004-present Facebook. All Rights Reserved.

#include <gmock/gmock.h>

#include <folly/io/async/AsyncSocket.h>

#include "src/ClientResumeStatusCallback.h"
#include "src/FrameTransport.h"
#include "src/NullRequestHandler.h"
#include "src/ReactiveSocket.h"
#include "src/folly/FollyKeepaliveTimer.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

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
      const std::shared_ptr<Subscription>& subscription) noexcept override {
    LOG(INFO) << "Subscription Paused";
  }

  void onSubscriptionResumed(
      const std::shared_ptr<Subscription>& subscription) noexcept override {
    LOG(INFO) << "Subscription Resumed";
  }

  void onSubscriberPaused(const std::shared_ptr<Subscriber<Payload>>&
                              subscriber) noexcept override {
    LOG(INFO) << "Subscriber Paused";
  }

  void onSubscriberResumed(const std::shared_ptr<Subscriber<Payload>>&
                               subscriber) noexcept override {
    LOG(INFO) << "Subscriber Resumed";
  }
};

class MySubscriber : public Subscriber<Payload> {
 public:

  void onSubscribe(std::shared_ptr<Subscription> sub) noexcept override {
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

  void onError(folly::exception_wrapper ex) noexcept override {}

  // methods for testing
  void request(size_t n) {
    subscription_->request(n);
  }

 private:
  std::shared_ptr<Subscription> subscription_;
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
          std::move(socket), inlineExecutor(), Stats::noop());
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
      Stats::noop(),
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
ConnectionSetupPayload getSetupPayload(ResumeIdentificationToken token) {
  return ConnectionSetupPayload(
      "text/plain", "text/plain", Payload("meta", "data"), true, token);
}
}
}

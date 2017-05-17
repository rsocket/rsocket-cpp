// Copyright 2004-present Facebook. All Rights Reserved.

#include "test/integration/ServerFixture.h"
#include <folly/Conv.h>
#include <gflags/gflags.h>
#include <gflags/gflags.h>

using namespace ::rsocket;
using namespace yarpl;

using folly::AsyncServerSocket;
using folly::AsyncSocket;
using folly::EventBase;
using folly::SocketAddress;
using testing::Test;

DEFINE_int32(port, 0, "Port to listen to");

namespace {

std::vector<
    std::pair<std::unique_ptr<ReactiveSocket>, ResumeIdentificationToken>>
    g_reactiveSockets;

class ServerSubscription : public yarpl::flowable::Subscription {
 public:
  explicit ServerSubscription(
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>> requester)
      : requester_(std::move(requester)) {}

  void request(int64_t n) noexcept override {
    LOG(INFO) << "Received request(" << n << ")";
    for (int64_t i = 0; i < n; i++) {
      VLOG(1) << "Sending " << sentCounter_ + 1;
      requester_->onNext(Payload(std::to_string(++sentCounter_)));
    }
  }

  void cancel() noexcept override {
    LOG(INFO) << "Received Cancel.  NOT IMPLEMENTED";
  }

 private:
  size_t sentCounter_{0};
  yarpl::Reference<yarpl::flowable::Subscriber<Payload>> requester_;
};

class ServerRequestHandler : public DefaultRequestHandler {
 public:
  void handleRequestStream(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          requester) noexcept override {
    LOG(INFO) << "Received RequestStream";
    requester->onSubscribe(make_ref<ServerSubscription>(requester));
  }

  void handleFireAndForgetRequest(
      Payload request,
      StreamId streamId) noexcept override {
    LOG(INFO) << "Received FireAndForget. NOT IMPLEMENTED";
  }

  void handleMetadataPush(
      std::unique_ptr<folly::IOBuf> request) noexcept override {
    LOG(INFO) << "Received MetadataPush. NOT IMPLEMENTED";
  }

  std::shared_ptr<StreamState> handleSetupPayload(
      SetupParameters request) noexcept override {
    LOG(INFO) << "Received SetupPayload. NOT IMPLEMENTED";
    return nullptr;
  }

  bool handleResume(ResumeParameters) noexcept override {
    LOG(INFO) << "Received Resume. NOT IMPLEMENTED";
    return false;
  }

  void handleCleanResume(yarpl::Reference<yarpl::flowable::Subscription>
                             response) noexcept override {
    LOG(INFO) << "Received CleanResume. NOT IMPLEMENTED";
  }

  void handleDirtyResume(yarpl::Reference<yarpl::flowable::Subscription>
                             response) noexcept override {
    LOG(INFO) << "Received DirtyResume. NOT IMPLEMENTED";
  }

  void onSubscriptionPaused(
      const yarpl::Reference<yarpl::flowable::Subscription>&
          subscription) noexcept override {
    LOG(INFO) << "SubscriptionPaused. NOT IMPLEMENTED";
  }

  void onSubscriptionResumed(
      const yarpl::Reference<yarpl::flowable::Subscription>&
          subscription) noexcept override {
    LOG(INFO) << "SubscriptionResumed. NOT IMPLEMENTED";
  }

  void onSubscriberPaused(
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          subscriber) noexcept override {
    LOG(INFO) << "SubscriberPaused. NOT IMPLEMENTED";
  }

  void onSubscriberResumed(
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          subscriber) noexcept override {
    LOG(INFO) << "SubscriberResumed. NOT IMPLEMENTED";
  }
};

class MyConnectionHandler : public ConnectionHandler {
 public:
  MyConnectionHandler(EventBase& eventBase) : eventBase_(eventBase) {}

  void setupNewSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      SetupParameters setupPayload) override {
    LOG(INFO) << "ServerSocket. SETUP socket from client";

    std::unique_ptr<RequestHandler> requestHandler =
        std::make_unique<ServerRequestHandler>();
    std::unique_ptr<ReactiveSocket> rs = ReactiveSocket::disconnectedServer(
        eventBase_, std::move(requestHandler), RSocketStats::noop());

    rs->onConnected([]() { LOG(INFO) << "ServerSocket Connected"; });
    rs->onDisconnected([rs = rs.get()](const folly::exception_wrapper& ex) {
      LOG(INFO) << "ServerSocket Disconnected: " << ex.what();
    });
    rs->onClosed([](const folly::exception_wrapper& ex) {
      LOG(INFO) << "ServerSocket Closed: " << ex.what();
    });
    rs->serverConnect(std::move(frameTransport), setupPayload);
    g_reactiveSockets.emplace_back(std::move(rs), setupPayload.token);
  }

  bool resumeSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ResumeParameters resumeParams) override {
    LOG(INFO) << "ServerSocket. RESUME socket from client ["
              << resumeParams.token << "]";
    CHECK_EQ(1, g_reactiveSockets.size());
    CHECK(g_reactiveSockets[0].second == resumeParams.token);
    auto result = g_reactiveSockets[0].first->tryResumeServer(
        frameTransport, resumeParams);
    LOG(INFO) << "Resume " << (result ? "SUCCEEDED" : "FAILED");
    return true;
  }

  void connectionError(
      std::shared_ptr<FrameTransport>,
      folly::exception_wrapper ex) override {
    LOG(WARNING) << "ServerSocket. ConnectionError: " << ex.what();
  }

 private:
  EventBase& eventBase_;
  std::shared_ptr<RSocketStats> stats_;
};

class MyAcceptCallback : public AsyncServerSocket::AcceptCallback {
 public:
  MyAcceptCallback(EventBase& eventBase)
      : eventBase_(eventBase),
        connectionHandler_(std::make_shared<MyConnectionHandler>(eventBase)),
        connectionAcceptor_(ProtocolVersion::Latest) {}

  virtual void connectionAccepted(
      int fd,
      const SocketAddress& clientAddr) noexcept override {
    LOG(INFO) << "Connection Accepted from " << clientAddr.describe();
    auto socket =
        folly::AsyncSocket::UniquePtr(new AsyncSocket(&eventBase_, fd));
    auto connection = std::make_unique<TcpDuplexConnection>(
        std::move(socket), inlineExecutor(), RSocketStats::noop());
    auto framedConnection = std::make_unique<FramedDuplexConnection>(
        std::move(connection), eventBase_);
    connectionAcceptor_.accept(std::move(framedConnection), connectionHandler_);
  }

  virtual void acceptError(const std::exception& ex) noexcept override {
    LOG(INFO) << "Connection Accept Error: " << ex.what();
  }

 private:
  EventBase& eventBase_;
  std::shared_ptr<MyConnectionHandler> connectionHandler_;
  ServerConnectionAcceptor connectionAcceptor_;
};
}

ServerFixture::ServerFixture()
    : myAcceptCallback_(std::make_unique<MyAcceptCallback>(eventBase_)) {
  serverAcceptThread_ = std::thread([=]() { eventBase_.loopForever(); });
  serverAcceptSocket_.reset(new AsyncServerSocket(&eventBase_));
  eventBase_.runInEventBaseThreadAndWait([=]() {
    folly::SocketAddress addr;
    addr.setFromLocalPort(folly::to<uint16_t>(FLAGS_port));
    serverAcceptSocket_->bind(addr);
    serverAcceptSocket_->addAcceptCallback(
        myAcceptCallback_.get(), &eventBase_);
    serverAcceptSocket_->listen(10);
    serverAcceptSocket_->startAccepting();
    LOG(INFO) << "Server listening on " << serverAcceptSocket_->getAddress();
    serverListenPort_ = serverAcceptSocket_->getAddress().getPort();
  });
}

ServerFixture::~ServerFixture() {
  eventBase_.runInEventBaseThreadAndWait([=]() {
    g_reactiveSockets.clear();
    serverAcceptSocket_.reset();
  });
  eventBase_.terminateLoopSoon();
  serverAcceptThread_.join();
}

// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <gmock/gmock.h>
#include "src/FrameTransport.h"
#include "src/NullRequestHandler.h"
#include "src/ServerConnectionAcceptor.h"
#include "src/StandardReactiveSocket.h"
#include "src/SubscriptionBase.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"
#include "test/simple/PrintSubscriber.h"
#include "test/simple/StatsPrinter.h"

using namespace ::testing;
using namespace ::reactivesocket;
using namespace ::folly;

DEFINE_string(address, "9898", "host:port to listen to");

namespace {

std::vector<
    std::pair<std::unique_ptr<ReactiveSocket>, ResumeIdentificationToken>>
    g_reactiveSockets;

class ServerSubscription : public SubscriptionBase {
 public:
  explicit ServerSubscription(std::shared_ptr<Subscriber<Payload>> response)
      : ExecutorBase(defaultExecutor()), response_(std::move(response)) {}

  ~ServerSubscription() {
    LOG(INFO) << "~ServerSubscription " << this;
  }

  // Subscription methods
  void requestImpl(size_t n) noexcept override {
    LOG(INFO) << "request " << this;
    response_->onNext(Payload("from server"));
    response_->onNext(Payload("from server2"));
    LOG(INFO) << "calling onComplete";
    if (auto response = std::move(response_)) {
      response->onComplete();
    }
    //    response_.onError(std::runtime_error("XXX"));
  }

  void cancelImpl() noexcept override {
    LOG(INFO) << "cancel " << this;
  }

 private:
  std::shared_ptr<Subscriber<Payload>> response_;
};

class ServerRequestHandler : public DefaultRequestHandler {
 public:
  explicit ServerRequestHandler(std::shared_ptr<StreamState> streamState)
      : streamState_(streamState) {}

  /// Handles a new inbound Stream requested by the other end.
  void handleRequestStream(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) noexcept override {
    LOG(INFO) << "ServerRequestHandler.handleRequestStream " << request;

    response->onSubscribe(std::make_shared<ServerSubscription>(response));
  }

  void handleFireAndForgetRequest(
      Payload request,
      StreamId streamId) noexcept override {
    LOG(INFO) << "ServerRequestHandler.handleFireAndForgetRequest " << request
              << "\n";
  }

  void handleMetadataPush(
      std::unique_ptr<folly::IOBuf> request) noexcept override {
    LOG(INFO) << "ServerRequestHandler.handleMetadataPush "
              << request->moveToFbString() << "\n";
  }

  std::shared_ptr<StreamState> handleSetupPayload(
      ReactiveSocket& socket,
      ConnectionSetupPayload request) noexcept override {
    CHECK(false) << "unexpected call";
    return nullptr;
  }

  bool handleResume(
      ReactiveSocket& socket,
      ResumeParameters) noexcept override {
    CHECK(false) << "unexpected call";
    return false;
  }

  void handleCleanResume(
      std::shared_ptr<Subscription> response) noexcept override {
    LOG(INFO) << "clean resume stream"
              << "\n";
  }

  void handleDirtyResume(
      std::shared_ptr<Subscription> response) noexcept override {
    LOG(INFO) << "dirty resume stream"
              << "\n";
  }

  void onSubscriptionPaused(
      const std::shared_ptr<Subscription>& subscription) noexcept override {
    LOG(INFO) << "subscription paused " << &subscription;
  }

  void onSubscriptionResumed(
      const std::shared_ptr<Subscription>& subscription) noexcept override {
    LOG(INFO) << "subscription resumed " << &subscription;
  }

  void onSubscriberPaused(const std::shared_ptr<Subscriber<Payload>>&
                              subscriber) noexcept override {
    LOG(INFO) << "subscriber paused " << &subscriber;
  }

  void onSubscriberResumed(const std::shared_ptr<Subscriber<Payload>>&
                               subscriber) noexcept override {
    LOG(INFO) << "subscriber resumed " << &subscriber;
  }

 private:
  // only keeping one
  std::shared_ptr<StreamState> streamState_;
};

class MyServerConnectionAcceptor : public ServerConnectionAcceptor {
 public:
  MyServerConnectionAcceptor(EventBase& eventBase, std::shared_ptr<Stats> stats)
      : eventBase_(eventBase), stats_(std::move(stats)) {}

  void setupNewSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ConnectionSetupPayload setupPayload,
      folly::Executor& executor) override {
    LOG(INFO) << "MyServerConnectionAcceptor::setupNewSocket " << setupPayload;

    std::unique_ptr<RequestHandler> requestHandler =
        std::make_unique<ServerRequestHandler>(nullptr);

    std::unique_ptr<StandardReactiveSocket> rs =
        StandardReactiveSocket::disconnectedServer(
            eventBase_, std::move(requestHandler), stats_);

    rs->onConnected([]() { LOG(INFO) << "socket connected"; });
    rs->onDisconnected([rs = rs.get()](const folly::exception_wrapper& ex) {
      LOG(INFO) << "socket disconnect: " << ex.what();
      // to verify these frames will be queued up
      rs->requestStream(
          Payload("from server resume"), std::make_shared<PrintSubscriber>());
    });
    rs->onClosed([](const folly::exception_wrapper& ex) {
      LOG(INFO) << "socket closed: " << ex.what();
    });

    if (g_reactiveSockets.empty()) {
      LOG(INFO) << "requestStream";
      rs->requestStream(
          Payload("from server"), std::make_shared<PrintSubscriber>());
    }

    LOG(INFO) << "serverConnecting ...";
    rs->serverConnect(std::move(frameTransport), true);

    LOG(INFO) << "RS " << rs.get();

    g_reactiveSockets.emplace_back(std::move(rs), setupPayload.token);
  }

  void resumeSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ResumeParameters resumeParams,
      folly::Executor& executor) override {
    LOG(INFO) << "MyServerConnectionAcceptor::resumeSocket resume token ["
              << resumeParams.token << "]";

    CHECK(g_reactiveSockets.size() == 1);
    CHECK(g_reactiveSockets[0].second == resumeParams.token);

    LOG(INFO) << "tryResumeServer...";
    auto result = g_reactiveSockets[0].first->tryResumeServer(
        frameTransport, resumeParams);
    LOG(INFO) << "resume " << (result ? "SUCCEEDED" : "FAILED");
  }

 private:
  EventBase& eventBase_;
  std::shared_ptr<Stats> stats_;
};

class Callback : public AsyncServerSocket::AcceptCallback {
 public:
  Callback(EventBase& eventBase, std::shared_ptr<Stats> stats)
      : eventBase_(eventBase),
        stats_(std::move(stats)),
        connectionAcceptor_(eventBase, stats) {}

  virtual void connectionAccepted(
      int fd,
      const SocketAddress& clientAddr) noexcept override {
    LOG(INFO) << "connectionAccepted" << clientAddr.describe();

    auto socket =
        folly::AsyncSocket::UniquePtr(new AsyncSocket(&eventBase_, fd));

    std::unique_ptr<DuplexConnection> connection =
        std::make_unique<TcpDuplexConnection>(
            std::move(socket), inlineExecutor(), stats_);
    std::unique_ptr<DuplexConnection> framedConnection =
        std::make_unique<FramedDuplexConnection>(
            std::move(connection), eventBase_);

    connectionAcceptor_.acceptConnection(
        std::move(framedConnection), eventBase_);
  }

  virtual void acceptError(const std::exception& ex) noexcept override {
    LOG(INFO) << "acceptError" << ex.what();
  }

  void shutdown() {
    shuttingDown = true;
    g_reactiveSockets.clear();
  }

 private:
  // only one for demo purposes. Should be token dependent.
  std::shared_ptr<StreamState> streamState_;
  EventBase& eventBase_;
  std::shared_ptr<Stats> stats_;
  bool shuttingDown{false};
  MyServerConnectionAcceptor connectionAcceptor_;
};
}

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;

#ifdef OSS
  google::ParseCommandLineFlags(&argc, &argv, true);
#else
  gflags::ParseCommandLineFlags(&argc, &argv, true);
#endif

  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  auto statsPrinter = std::make_shared<reactivesocket::StatsPrinter>();

  EventBase eventBase;
  auto thread = std::thread([&eventBase]() { eventBase.loopForever(); });

  Callback callback(eventBase, statsPrinter);

  auto serverSocket = AsyncServerSocket::newSocket(&eventBase);

  eventBase.runInEventBaseThreadAndWait(
      [&callback, &eventBase, &serverSocket]() {
        folly::SocketAddress addr;
        addr.setFromLocalIpPort(FLAGS_address);

        serverSocket->bind(addr);
        serverSocket->addAcceptCallback(&callback, &eventBase);
        serverSocket->listen(10);
        serverSocket->startAccepting();

        LOG(INFO) << "server listening on ";
        for (auto i : serverSocket->getAddresses())
          LOG(INFO) << i.describe() << ' ';
      });

  std::string name;
  std::getline(std::cin, name);

  eventBase.runInEventBaseThreadAndWait([&callback]() { callback.shutdown(); });
  eventBase.terminateLoopSoon();

  thread.join();
}

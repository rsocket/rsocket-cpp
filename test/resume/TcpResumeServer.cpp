// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <folly/portability/GFlags.h>
#include <gmock/gmock.h>

#include "src/FrameTransport.h"
#include "src/NullRequestHandler.h"
#include "src/ServerConnectionAcceptor.h"
#include "src/ReactiveSocket.h"
#include "src/SubscriptionBase.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"
#include "test/simple/PrintSubscriber.h"
#include "test/simple/StatsPrinter.h"

using namespace ::testing;
using namespace ::reactivesocket;
using namespace ::folly;
using namespace yarpl;

DEFINE_string(address, "9898", "host:port to listen to");

namespace {

std::vector<
    std::pair<std::unique_ptr<ReactiveSocket>, ResumeIdentificationToken>>
    g_reactiveSockets;

class ServerSubscription : public yarpl::flowable::Subscription {
 public:
  explicit ServerSubscription(yarpl::Reference<yarpl::flowable::Subscriber<Payload>> response)
      : response_(std::move(response)) {}

  ~ServerSubscription() {
    LOG(INFO) << "~ServerSubscription " << this;
  }

  // Subscription methods
  void request(int64_t n) noexcept override {
    LOG(INFO) << "request " << this;
    response_->onNext(Payload("from server"));
    response_->onNext(Payload("from server2"));
    LOG(INFO) << "calling onComplete";
    if (auto response = std::move(response_)) {
      response->onComplete();
    }
    //    response_.onError(std::runtime_error("XXX"));
  }

  void cancel() noexcept override {
    LOG(INFO) << "cancel " << this;
  }

 private:
  yarpl::Reference<yarpl::flowable::Subscriber<Payload>> response_;
};

class ServerRequestHandler : public DefaultRequestHandler {
 public:
  explicit ServerRequestHandler(std::shared_ptr<StreamState> streamState)
      : streamState_(streamState) {}

  /// Handles a new inbound Stream requested by the other end.
  void handleRequestStream(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>& response) noexcept override {
    LOG(INFO) << "ServerRequestHandler.handleRequestStream " << request;

    response->onSubscribe(make_ref<ServerSubscription>(response));
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
      yarpl::Reference<yarpl::flowable::Subscription> response) noexcept override {
    LOG(INFO) << "clean resume stream"
              << "\n";
  }

  void handleDirtyResume(
      yarpl::Reference<yarpl::flowable::Subscription> response) noexcept override {
    LOG(INFO) << "dirty resume stream"
              << "\n";
  }

  void onSubscriptionPaused(
      const yarpl::Reference<yarpl::flowable::Subscription>& subscription) noexcept override {
    LOG(INFO) << "subscription paused " << &subscription;
  }

  void onSubscriptionResumed(
      const yarpl::Reference<yarpl::flowable::Subscription>& subscription) noexcept override {
    LOG(INFO) << "subscription resumed " << &subscription;
  }

  void onSubscriberPaused(const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
                              subscriber) noexcept override {
    LOG(INFO) << "subscriber paused " << &subscriber;
  }

  void onSubscriberResumed(const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
                               subscriber) noexcept override {
    LOG(INFO) << "subscriber resumed " << &subscriber;
  }

 private:
  // only keeping one
  std::shared_ptr<StreamState> streamState_;
};

class MyConnectionHandler : public ConnectionHandler {
 public:
  MyConnectionHandler(EventBase& eventBase, std::shared_ptr<Stats> stats)
      : eventBase_(eventBase), stats_(std::move(stats)) {}

  void setupNewSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ConnectionSetupPayload setupPayload) override {
    LOG(INFO) << "MyConnectionHandler::setupNewSocket " << setupPayload;

    std::unique_ptr<RequestHandler> requestHandler =
        std::make_unique<ServerRequestHandler>(nullptr);

    std::unique_ptr<ReactiveSocket> rs =
        ReactiveSocket::disconnectedServer(
            eventBase_, std::move(requestHandler), stats_);

    rs->onConnected([]() { LOG(INFO) << "socket connected"; });
    rs->onDisconnected([rs = rs.get()](const folly::exception_wrapper& ex) {
      LOG(INFO) << "socket disconnect: " << ex.what();
      // to verify these frames will be queued up
      rs->requestStream(
          Payload("from server resume"), make_ref<PrintSubscriber>());
    });
    rs->onClosed([](const folly::exception_wrapper& ex) {
      LOG(INFO) << "socket closed: " << ex.what();
    });

    if (g_reactiveSockets.empty()) {
      LOG(INFO) << "requestStream";
      // not permited to make requests at this moment
      // rs->requestStream(
      //     Payload("from server"), std::make_shared<PrintSubscriber>());
    }

    LOG(INFO) << "serverConnecting ...";
    rs->serverConnect(std::move(frameTransport), setupPayload);

    LOG(INFO) << "RS " << rs.get();

    g_reactiveSockets.emplace_back(std::move(rs), setupPayload.token);
  }

  bool resumeSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ResumeParameters resumeParams) override {
    LOG(INFO) << "MyConnectionHandler::resumeSocket resume token ["
              << resumeParams.token << "]";

    CHECK_EQ(1, g_reactiveSockets.size());
    CHECK(g_reactiveSockets[0].second == resumeParams.token);

    LOG(INFO) << "tryResumeServer...";
    auto result = g_reactiveSockets[0].first->tryResumeServer(
        frameTransport, resumeParams);
    LOG(INFO) << "resume " << (result ? "SUCCEEDED" : "FAILED");

    return true;
  }

  void connectionError(
      std::shared_ptr<FrameTransport>,
      folly::exception_wrapper ex) override {
    LOG(WARNING) << "Connection failed: " << ex.what();
  }

 private:
  EventBase& eventBase_;
  std::shared_ptr<Stats> stats_;
};

class Callback : public AsyncServerSocket::AcceptCallback {
 public:
  Callback(EventBase& eventBase, std::shared_ptr<Stats> stats)
      : eventBase_(eventBase),
        stats_(stats),
        connectionHandler_(
            std::make_shared<MyConnectionHandler>(eventBase, stats)),
        connectionAcceptor_(ProtocolVersion::Unknown) {}

  virtual void connectionAccepted(
      int fd,
      const SocketAddress& clientAddr) noexcept override {
    LOG(INFO) << "connectionAccepted" << clientAddr.describe();

    auto socket =
        folly::AsyncSocket::UniquePtr(new AsyncSocket(&eventBase_, fd));

    auto connection = std::make_unique<TcpDuplexConnection>(
        std::move(socket), inlineExecutor(), stats_);
    auto framedConnection = std::make_unique<FramedDuplexConnection>(
        std::move(connection), ProtocolVersion::Unknown, eventBase_);

    connectionAcceptor_.accept(std::move(framedConnection), connectionHandler_);
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
  std::shared_ptr<MyConnectionHandler> connectionHandler_;
  ServerConnectionAcceptor connectionAcceptor_;
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

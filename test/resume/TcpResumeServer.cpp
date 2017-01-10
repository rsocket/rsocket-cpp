// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <gmock/gmock.h>
#include "src/FrameTransport.h"
#include "src/NullRequestHandler.h"
#include "src/SmartPointers.h"
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

std::vector<std::pair<
    std::unique_ptr<StandardReactiveSocket>,
    ResumeIdentificationToken>>
    g_reactiveSockets;

class ServerSubscription : public SubscriptionBase {
 public:
  explicit ServerSubscription(std::shared_ptr<Subscriber<Payload>> response)
      : ExecutorBase(defaultExecutor()), response_(std::move(response)) {}

  ~ServerSubscription() {
    LOG(INFO) << "~ServerSubscription " << this;
  }

  // Subscription methods
  void requestImpl(size_t n) override {
    LOG(INFO) << "request " << this;
    response_.onNext(Payload("from server"));
    response_.onNext(Payload("from server2"));
    LOG(INFO) << "calling onComplete";
    response_.onComplete();
    //    response_.onError(std::runtime_error("XXX"));
  }

  void cancelImpl() override {
    LOG(INFO) << "cancel " << this;
  }

 private:
  SubscriberPtr<Subscriber<Payload>> response_;
};

class ServerRequestHandler : public DefaultRequestHandler {
 public:
  explicit ServerRequestHandler(std::shared_ptr<StreamState> streamState)
      : streamState_(streamState) {}

  /// Handles a new inbound Subscription requested by the other end.
  void handleRequestSubscription(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) override {
    LOG(INFO) << "ServerRequestHandler.handleRequestSubscription " << request;
    response->onSubscribe(std::make_shared<ServerSubscription>(response));
  }

  /// Handles a new inbound Stream requested by the other end.
  void handleRequestStream(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) override {
    LOG(INFO) << "ServerRequestHandler.handleRequestStream " << request;

    response->onSubscribe(std::make_shared<ServerSubscription>(response));
  }

  void handleFireAndForgetRequest(Payload request, StreamId streamId) override {
    LOG(INFO) << "ServerRequestHandler.handleFireAndForgetRequest " << request
              << "\n";
  }

  void handleMetadataPush(std::unique_ptr<folly::IOBuf> request) override {
    LOG(INFO) << "ServerRequestHandler.handleMetadataPush "
              << request->moveToFbString() << "\n";
  }

  std::shared_ptr<StreamState> handleSetupPayload(
      ConnectionSetupPayload request) override {
    std::stringstream str;

    str << "ServerRequestHandler.handleSetupPayload " << request
        << " setup token <";
    for (uint8_t byte : request.token.data()) {
      str << (int)byte;
    }
    str << "> " << streamState_.get() << " " << streamState_->streams_.size()
        << "\n";
    LOG(INFO) << str.str();
    // TODO: we need to get ReactiveSocket pointer somehow
    g_reactiveSockets[0].second = request.token;
    return streamState_;
  }

  std::shared_ptr<StreamState> handleResume(
      const ResumeIdentificationToken& token,
      ResumePosition position) override {
    std::stringstream str;

    str << "ServerRequestHandler.handleResume resume token <";
    for (uint8_t byte : token.data()) {
      str << (int)byte;
    }
    str << "> " << streamState_.get() << " " << streamState_->streams_.size()
        << "\n";

    LOG(INFO) << str.str();

    CHECK(g_reactiveSockets.size() == 2);
    CHECK(g_reactiveSockets[0].second == token);

    LOG(INFO) << "detaching frame transport";
    auto frameTransport = g_reactiveSockets[1].first->detachFrameTransport();
    LOG(INFO) << "tryResumeServer...";
    auto result =
        g_reactiveSockets[0].first->tryResumeServer(frameTransport, position);
    LOG(INFO) << "resume " << (result ? "SUCCEEDED" : "FAILED");

    // TODO(lehecka): unused, make it used again
    // return streamState_;
    return nullptr;
  }

  void handleCleanResume(std::shared_ptr<Subscription> response) override {
    LOG(INFO) << "clean resume stream"
              << "\n";
  }

  void handleDirtyResume(std::shared_ptr<Subscription> response) override {
    LOG(INFO) << "dirty resume stream"
              << "\n";
  }

 private:
  // only keeping one
  std::shared_ptr<StreamState> streamState_;
};

class Callback : public AsyncServerSocket::AcceptCallback {
 public:
  Callback(EventBase& eventBase, Stats& stats)
      : streamState_(std::make_shared<StreamState>()),
        eventBase_(eventBase),
        stats_(stats){};

  virtual ~Callback() = default;

  virtual void connectionAccepted(
      int fd,
      const SocketAddress& clientAddr) noexcept override {
    LOG(INFO) << "connectionAccepted" << clientAddr.describe();

    auto socket =
        folly::AsyncSocket::UniquePtr(new AsyncSocket(&eventBase_, fd));

    std::unique_ptr<DuplexConnection> connection =
        folly::make_unique<TcpDuplexConnection>(std::move(socket), stats_);
    std::unique_ptr<DuplexConnection> framedConnection =
        folly::make_unique<FramedDuplexConnection>(std::move(connection));
    std::unique_ptr<RequestHandler> requestHandler =
        folly::make_unique<ServerRequestHandler>(streamState_);

    std::unique_ptr<StandardReactiveSocket> rs =
        StandardReactiveSocket::disconnectedServer(
            eventBase_, std::move(requestHandler), stats_);

    rs->onConnected([](StandardReactiveSocket& socket) {
      LOG(INFO) << "socket connected " << &socket;
    });
    rs->onDisconnected([](StandardReactiveSocket& socket) {
      LOG(INFO) << "socket disconnect " << &socket;
      // to verify these frames will be queued up
      socket.requestStream(
          Payload("from server resume"), std::make_shared<PrintSubscriber>());
    });
    rs->onClosed([](StandardReactiveSocket& socket) {
      LOG(INFO) << "socket closed " << &socket;
    });

    if (g_reactiveSockets.empty()) {
      LOG(INFO) << "requestStream";
      rs->requestStream(
          Payload("from server"), std::make_shared<PrintSubscriber>());
    }

    LOG(INFO) << "serverConnecting ...";
    rs->serverConnect(
        std::make_shared<FrameTransport>(std::move(framedConnection)), true);

    LOG(INFO) << "RS " << rs.get();

    g_reactiveSockets.emplace_back(
        std::move(rs), ResumeIdentificationToken::generateNew());
  }

  void removeSocket(StandardReactiveSocket& socket) {
    if (!shuttingDown) {
      g_reactiveSockets.erase(std::remove_if(
          g_reactiveSockets.begin(),
          g_reactiveSockets.end(),
          [&socket](const std::pair<
                    std::unique_ptr<StandardReactiveSocket>,
                    ResumeIdentificationToken>& kv) {
            return kv.first.get() == &socket;
          }));
    }
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
  Stats& stats_;
  bool shuttingDown{false};
};
}

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;

  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  reactivesocket::StatsPrinter statsPrinter;

  EventBase eventBase;
  auto thread = std::thread([&eventBase]() { eventBase.loopForever(); });

  Callback callback(eventBase, statsPrinter);

  auto serverSocket = AsyncServerSocket::newSocket(&eventBase);

  eventBase.runInEventBaseThreadAndWait(
      [&callback, &eventBase, &serverSocket]() {
        folly::SocketAddress addr;
        addr.setFromLocalIpPort(FLAGS_address);

        serverSocket->setReusePortEnabled(true);
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

// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <gmock/gmock.h>
#include "src/NullRequestHandler.h"
#include "src/ReactiveSocket.h"
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
class ServerSubscription : public SubscriptionBase {
 public:
  explicit ServerSubscription(std::shared_ptr<Subscriber<Payload>> response)
      : response_(std::move(response)) {}

  ~ServerSubscription(){};

  // Subscription methods
  void requestImpl(size_t n) override {
    response_.onNext(Payload("from server"));
    response_.onNext(Payload("from server2"));
    response_.onComplete();
    //    response_.onError(std::runtime_error("XXX"));
  }

  void cancelImpl() override {}

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
    for (uint8_t byte : request.token) {
      str << (int)byte;
    }
    str << "> " << streamState_.get() << " " << streamState_->streams_.size()
        << "\n";
    LOG(INFO) << str.str();
    return streamState_;
  }

  std::shared_ptr<StreamState> handleResume(
      const ResumeIdentificationToken& token) override {
    std::stringstream str;

    str << "ServerRequestHandler.handleResume resume token <";
    for (uint8_t byte : token) {
      str << (int)byte;
    }
    str << "> " << streamState_.get() << " " << streamState_->streams_.size()
        << "\n";

    LOG(INFO) << str.str();
    return streamState_;
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
    std::cout << "connectionAccepted" << clientAddr.describe() << "\n";

    auto socket =
        folly::AsyncSocket::UniquePtr(new AsyncSocket(&eventBase_, fd));

    std::unique_ptr<DuplexConnection> connection =
        folly::make_unique<TcpDuplexConnection>(std::move(socket), stats_);
    std::unique_ptr<DuplexConnection> framedConnection =
        folly::make_unique<FramedDuplexConnection>(std::move(connection));
    std::unique_ptr<RequestHandler> requestHandler =
        folly::make_unique<ServerRequestHandler>(streamState_);

    std::unique_ptr<ReactiveSocket> rs = ReactiveSocket::fromServerConnection(
        std::move(framedConnection), std::move(requestHandler), stats_);

    std::cout << "RS " << rs.get() << std::endl;

    // keep the ReactiveSocket around so it can be resumed
    //    rs->onClose(
    //        std::bind(&Callback::removeSocket, this, std::placeholders::_1));

    reactiveSockets_.push_back(std::move(rs));
  }

  void removeSocket(ReactiveSocket& socket) {
    if (!shuttingDown) {
      reactiveSockets_.erase(std::remove_if(
          reactiveSockets_.begin(),
          reactiveSockets_.end(),
          [&socket](std::unique_ptr<ReactiveSocket>& vecSocket) {
            return vecSocket.get() == &socket;
          }));
    }
  }

  virtual void acceptError(const std::exception& ex) noexcept override {
    std::cout << "acceptError" << ex.what() << "\n";
  }

  void shutdown() {
    shuttingDown = true;
    reactiveSockets_.clear();
  }

 private:
  // only one for demo purposes. Should be token dependent.
  std::shared_ptr<StreamState> streamState_;
  std::vector<std::unique_ptr<ReactiveSocket>> reactiveSockets_;
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

        std::cout << "server listening on ";
        for (auto i : serverSocket->getAddresses())
          std::cout << i.describe() << ' ';
        std::cout << '\n';
      });

  std::string name;
  std::getline(std::cin, name);

  eventBase.runInEventBaseThreadAndWait([&callback]() { callback.shutdown(); });
  eventBase.terminateLoopSoon();

  thread.join();
}

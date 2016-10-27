// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <gmock/gmock.h>
#include <thread>
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
  /// Handles a new inbound Subscription requested by the other end.
  void handleRequestSubscription(
      Payload request,
      const std::shared_ptr<Subscriber<Payload>>& response) override {
    LOG(INFO) << "ServerRequestHandler.handleRequestSubscription " << request;

    response->onSubscribe(std::make_shared<ServerSubscription>(response));
  }

  /// Handles a new inbound Stream requested by the other end.
  void handleRequestStream(
      Payload request,
      const std::shared_ptr<Subscriber<Payload>>& response) override {
    LOG(INFO) << "ServerRequestHandler.handleRequestStream " << request;

    response->onSubscribe(std::make_shared<ServerSubscription>(response));
  }

  void handleFireAndForgetRequest(Payload request) override {
    LOG(INFO) << "ServerRequestHandler.handleFireAndForgetRequest " << request
              << "\n";
  }

  void handleMetadataPush(std::unique_ptr<folly::IOBuf> request) override {
    LOG(INFO) << "ServerRequestHandler.handleMetadataPush "
              << request->moveToFbString() << "\n";
  }

  void handleSetupPayload(ConnectionSetupPayload request) override {
    LOG(INFO) << "ServerRequestHandler.handleSetupPayload " << request << "\n";
  }
};

class Callback : public AsyncServerSocket::AcceptCallback {
 public:
  Callback(EventBase& eventBase, Stats& stats)
      : eventBase_(eventBase), stats_(stats){};

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
        folly::make_unique<ServerRequestHandler>();

    std::unique_ptr<ReactiveSocket> rs = ReactiveSocket::fromServerConnection(
        std::move(framedConnection),
        std::move(requestHandler),
        stats_,
        [&](ReactiveSocket& socket,
            const ResumeIdentificationToken& token,
            ResumePosition position) {
          std::cout << "resume callback token <";
          for (uint8_t byte : token) {
            std::cout << (int)byte;
          }

          std::cout << "> RS " << &socket << " " << reactiveSockets_[0].get();
          std::cout << " position difference "
                    << reactiveSockets_[0]->positionDifference(position);
          std::cout << " isAvailable "
                    << reactiveSockets_[0]->isPositionAvailable(position)
                    << std::endl;
          socket.resumeFromSocket(*reactiveSockets_[0]);
          return true;
        });

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

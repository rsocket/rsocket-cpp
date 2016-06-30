#include <folly/Memory.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <gmock/gmock.h>
#include <reactivesocket-cpp/src/ReactiveSocket.h>
#include <reactivesocket-cpp/src/RequestHandler.h>
#include <reactivesocket-cpp/src/framed/FramedDuplexConnection.h>
#include <reactivesocket-cpp/src/mixins/MemoryMixin.h>
#include <reactivesocket-cpp/test/simple/CancelSubscriber.h>
#include <reactivesocket-cpp/test/simple/NullSubscription.h>
#include <reactivesocket-cpp/test/simple/PrintSubscriber.h>
#include <thread>
#include "reactivesocket-cpp/src/tcp/TcpDuplexConnection.h"

using namespace ::testing;
using namespace ::reactivesocket;
using namespace ::folly;

DEFINE_string(address, "9898", "host:port to listen to");

namespace {
class ServerSubscription : public virtual IntrusiveDeleter,
                           public Subscription {
 public:
  ~ServerSubscription() {}

  // Subscription methods
  void request(size_t n) override {
    // TODO delay sending responses until this is triggered
  }

  void cancel() override {}
};

class ServerRequestHandler : public RequestHandler {
 public:
  /// Handles a new Channel requested by the other end.
  ///
  /// Modelled after Producer::subscribe, hence must synchronously call
  /// Subscriber::onSubscribe, and provide a valid Subscription.
  Subscriber<Payload>& handleRequestChannel(
      reactivesocket::Payload request,
      Subscriber<Payload>& response) override {
    LOG(ERROR) << "not expecting server call";
    response.onError(std::runtime_error("incoming channel not supported"));

    auto* subscription = new MemoryMixin<NullSubscription>();
    response.onSubscribe(*subscription);

    return *(new MemoryMixin<CancelSubscriber>());
  }

  /// Handles a new inbound Subscription requested by the other end.
  void handleRequestSubscription(Payload request, Subscriber<Payload>& response)
      override {
    auto* subscription = new MemoryMixin<ServerSubscription>();
    response.onSubscribe(*subscription);

    std::cout << "ServerRequestHandler.handleRequestSubscription "
              << request->moveToFbString() << "\n";

    response.onNext(folly::IOBuf::copyBuffer("from server"));
    response.onNext(folly::IOBuf::copyBuffer("from server2"));
    response.onComplete();
  }
};

class Callback : public AsyncServerSocket::AcceptCallback {
 public:
  Callback(EventBase& eventBase) : eventBase_(eventBase){};

  virtual ~Callback() = default;

  virtual void connectionAccepted(
      int fd,
      const SocketAddress& clientAddr) noexcept override {
    std::cout << "connectionAccepted" << clientAddr.describe() << "\n";

    auto socket =
        folly::AsyncSocket::UniquePtr(new AsyncSocket(&eventBase_, fd));

    std::unique_ptr<DuplexConnection> connection =
        folly::make_unique<TcpDuplexConnection>(std::move(socket));
    std::unique_ptr<DuplexConnection> framedConnection =
        folly::make_unique<FramedDuplexConnection>(std::move(connection));
    std::unique_ptr<RequestHandler> requestHandler =
        folly::make_unique<ServerRequestHandler>();

    reactiveSocket_ = ReactiveSocket::fromServerConnection(
        std::move(framedConnection), std::move(requestHandler));
  }

  virtual void acceptError(const std::exception& ex) noexcept override {
    std::cout << "acceptError" << ex.what() << "\n";
  }

  void shutdown() {
    reactiveSocket_.reset(nullptr);
  }

 private:
  std::unique_ptr<ReactiveSocket> reactiveSocket_;
  EventBase& eventBase_;
};
}

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  EventBase eventBase;
  auto thread = std::thread([&]() { eventBase.loopForever(); });

  Callback callback(eventBase);

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
}

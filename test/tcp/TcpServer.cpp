#include <folly/Memory.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <gmock/gmock.h>
#include <thread>
#include "src/NullRequestHandler.h"
#include "src/ReactiveSocket.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/mixins/MemoryMixin.h"
#include "src/tcp/TcpDuplexConnection.h"
#include "test/simple/PrintSubscriber.h"
#include "test/simple/StatsPrinter.h"

using namespace ::testing;
using namespace ::reactivesocket;
using namespace ::folly;

DEFINE_string(address, "9898", "host:port to listen to");

namespace {
class ServerSubscription : public virtual IntrusiveDeleter,
                           public Subscription {
 public:
  explicit ServerSubscription(Subscriber<Payload>& response)
      : response_(response) {}

  ~ServerSubscription(){};

  // Subscription methods
  void request(size_t n) override {
    response_.onNext(folly::IOBuf::copyBuffer("from server"));
    //    response.onNext(folly::IOBuf::copyBuffer("from server2"));
    //    response.onComplete();
    response_.onError(std::runtime_error("XXX"));
  }

  void cancel() override {}

 private:
  Subscriber<Payload>& response_;
};

class ServerRequestHandler : public DefaultRequestHandler {
 public:
  /// Handles a new inbound Subscription requested by the other end.
  void handleRequestSubscription(Payload request, Subscriber<Payload>& response)
      override {
    LOG(INFO) << "ServerRequestHandler.handleRequestSubscription "
              << request->moveToFbString();

    response.onSubscribe(createManagedInstance<ServerSubscription>(response));
  }

  void handleFireAndForgetRequest(Payload request) override {
    LOG(INFO) << "ServerRequestHandler.handleFireAndForgetRequest "
              << request->moveToFbString() << "\n";
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
        folly::make_unique<FramedDuplexConnection>(
            std::move(connection), stats_);
    std::unique_ptr<RequestHandler> requestHandler =
        folly::make_unique<ServerRequestHandler>();

    reactiveSocket_ = ReactiveSocket::fromServerConnection(
        std::move(framedConnection), std::move(requestHandler), stats_);
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
  Stats& stats_;
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
  auto thread = std::thread([&]() { eventBase.loopForever(); });

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

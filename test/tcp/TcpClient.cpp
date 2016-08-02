#include <folly/Memory.h>
#include <gmock/gmock.h>
#include <thread>
#include "src/ReactiveSocket.h"
#include "src/RequestHandler.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/mixins/MemoryMixin.h"
#include "src/tcp/TcpDuplexConnection.h"
#include "test/simple/CancelSubscriber.h"
#include "test/simple/NullSubscription.h"
#include "test/simple/PrintSubscriber.h"
#include "test/simple/StatsPrinter.h"

using namespace ::testing;
using namespace ::reactivesocket;
using namespace ::folly;

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

namespace {
class ClientRequestHandler : public RequestHandler {
 public:
  ~ClientRequestHandler() {
    LOG(INFO) << "~ClientRequestHandler()";
  }

  /// Handles a new Channel requested by the other end.
  ///
  /// Modelled after Producer::subscribe, hence must synchronously call
  /// Subscriber::onSubscribe, and provide a valid Subscription.
  Subscriber<Payload>& handleRequestChannel(
      Payload request,
      Subscriber<Payload>& response) override {
    LOG(ERROR) << "not expecting server call";
    response.onError(std::runtime_error("incoming request not supported"));
    response.onSubscribe(createManagedInstance<NullSubscription>());
    return createManagedInstance<CancelSubscriber>();
  }

  /// Handles a new inbound Subscription requested by the other end.
  void handleRequestSubscription(Payload request, Subscriber<Payload>& response)
      override {
    LOG(ERROR) << "not expecting server call";
    response.onError(std::runtime_error("incoming request not supported"));
    response.onSubscribe(createManagedInstance<NullSubscription>());
  }

  void handleFireAndForgetRequest(Payload request) override {
    LOG(ERROR) << "not expecting server call";
  }
};

class Callback : public AsyncSocket::ConnectCallback {
 public:
  virtual ~Callback() = default;

  void connectSuccess() noexcept override {}

  void connectErr(const AsyncSocketException& ex) noexcept override {
    std::cout << "TODO error" << ex.what() << " " << ex.getType() << "\n";
  }
};
}

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;

  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  EventBase eventBase;
  auto thread = std::thread([&]() { eventBase.loopForever(); });

  std::unique_ptr<ReactiveSocket> reactiveSocket;
  Callback callback;
  StatsPrinter stats;

  eventBase.runInEventBaseThreadAndWait(
      [&callback, &reactiveSocket, &eventBase, &stats]() {
        folly::AsyncSocket::UniquePtr socket(
            new folly::AsyncSocket(&eventBase));

        folly::SocketAddress addr(FLAGS_host, FLAGS_port, true);

        socket->connect(&callback, addr);

        std::cout << "attempting connection to " << addr.describe() << "\n";

        std::unique_ptr<DuplexConnection> connection =
            folly::make_unique<TcpDuplexConnection>(std::move(socket), stats);
        std::unique_ptr<DuplexConnection> framedConnection =
            folly::make_unique<FramedDuplexConnection>(
                std::move(connection), stats);
        std::unique_ptr<RequestHandler> requestHandler =
            folly::make_unique<ClientRequestHandler>();

        reactiveSocket = ReactiveSocket::fromClientConnection(
            std::move(framedConnection), std::move(requestHandler), stats);

        reactiveSocket->requestSubscription(
            folly::IOBuf::copyBuffer("from client"),
            createManagedInstance<PrintSubscriber>());
      });

  std::string name;
  std::getline(std::cin, name);

  // TODO why need to shutdown in eventbase?
  eventBase.runInEventBaseThreadAndWait(
      [&reactiveSocket]() { reactiveSocket.reset(nullptr); });

  eventBase.terminateLoopSoon();
  thread.join();

  return 0;
}

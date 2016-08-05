#include <folly/Memory.h>
#include <folly/io/async/EventBaseManager.h>
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

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

namespace {
class Callback : public AsyncSocket::ConnectCallback {
 public:
  virtual ~Callback() = default;

  void connectSuccess() noexcept override {}

  void connectErr(const AsyncSocketException& ex) noexcept override {
    std::cout << "TODO error" << ex.what() << " " << ex.getType() << "\n";
  }
};

class FollyKeepaliveTimer : public KeepaliveTimer {
 public:
  explicit FollyKeepaliveTimer(folly::EventBase& eventBase)
      : eventBase_(eventBase) {
    running_ = std::make_shared<bool>(false);
  }

  std::chrono::milliseconds keepaliveTime() override {
    return std::chrono::milliseconds(5000);
  };

  void schedule() {
    auto running = running_;
    eventBase_.runAfterDelay(
        [this, running]() {
          if (*running) {
            automaton_->sendKeepalive();
            schedule();
          }
        },
        keepaliveTime().count());
  }

  void stop() override {
    *running_ = false;
  };

  void start(ConnectionAutomaton* automaton) override {
    automaton_ = automaton;
    *running_ = true;

    schedule();
  };

 private:
  ConnectionAutomaton* automaton_{nullptr};
  folly::EventBase& eventBase_;
  std::shared_ptr<bool> running_;
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
            folly::make_unique<DefaultRequestHandler>();

        reactiveSocket = ReactiveSocket::fromClientConnection(
            std::move(framedConnection),
            std::move(requestHandler),
            stats,
            folly::make_unique<FollyKeepaliveTimer>(eventBase));

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

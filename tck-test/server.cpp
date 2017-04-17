// Copyright 2004-present Facebook. All Rights Reserved.

#include <fstream>

#include <folly/Memory.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <gmock/gmock.h>

#include "src/NullRequestHandler.h"
#include "src/StandardReactiveSocket.h"
#include "src/SubscriptionBase.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

#include "test/simple/StatsPrinter.h"

#include "tck-test/MarbleProcessor.h"

using namespace ::testing;
using namespace ::reactivesocket;
using namespace ::folly;

DEFINE_string(ip, "0.0.0.0", "IP to bind on");
DEFINE_int32(port, 9898, "port to listen to");
DEFINE_string(test_file, "../tck-test/servertest.txt", "test file to run");
DEFINE_bool(enable_stats_printer, false, "enable StatsPrinter");

namespace {

struct MarbleStore {
  std::map<std::pair<std::string, std::string>, std::string> reqRespMarbles;
  std::map<std::pair<std::string, std::string>, std::string> streamMarbles;
  std::map<std::pair<std::string, std::string>, std::string> channelMarbles;
};

MarbleStore parseMarbles(const std::string& fileName) {
  MarbleStore ms;

  std::ifstream input(fileName);
  if (!input.good()) {
    LOG(FATAL) << "Could not read from file '" << fileName << "'";
  }

  std::string line;
  while (std::getline(input, line)) {
    std::vector<folly::StringPiece> args;
    folly::split("%%", line, args);
    CHECK(args.size() == 4);
    if (args[0] == "rr") {
      ms.reqRespMarbles.emplace(
          std::make_pair(args[1].toString(), args[2].toString()),
          args[3].toString());
    } else if (args[0] == "rs") {
      ms.streamMarbles.emplace(
          std::make_pair(args[1].toString(), args[2].toString()),
          args[3].toString());
    } else if (args[0] == "channel") {
      ms.channelMarbles.emplace(
          std::make_pair(args[1].toString(), args[2].toString()),
          args[3].toString());
    } else {
      LOG(FATAL) << "Unrecognized token " << args[0];
    }
  }
  return ms;
}

class ServerSubscription : public SubscriptionBase {
 public:
  explicit ServerSubscription(
      std::shared_ptr<Subscriber<Payload>> response,
      std::shared_ptr<tck::MarbleProcessor> marbleProcessor,
      size_t /* numElems = 2 */)
      : ExecutorBase(defaultExecutor()),
        response_(std::move(response)),
        marbleProcessor_(std::move(marbleProcessor)) {}

 private:
  void requestImpl(size_t n) noexcept override {
    marbleProcessor_->request(n);
  }

  void cancelImpl() noexcept override {
    marbleProcessor_->cancel();
  }

  std::shared_ptr<Subscriber<Payload>> response_;
  std::shared_ptr<tck::MarbleProcessor> marbleProcessor_;
};

class Callback : public AsyncServerSocket::AcceptCallback {
 public:
  explicit Callback(EventBase& eventBase) : eventBase_(eventBase) {}

  virtual ~Callback() = default;

  virtual void connectionAccepted(
      int fd,
      const SocketAddress& clientAddr) noexcept override {
    LOG(INFO) << "ConnectionAccepted " << clientAddr.describe() << std::endl;

    auto socket =
        folly::AsyncSocket::UniquePtr(new AsyncSocket(&eventBase_, fd));

    std::shared_ptr<Stats> stats;

    if (FLAGS_enable_stats_printer) {
      stats.reset(new reactivesocket::StatsPrinter());
    } else {
      stats = Stats::noop();
    }

    std::unique_ptr<DuplexConnection> connection =
        std::make_unique<TcpDuplexConnection>(
            std::move(socket), inlineExecutor(), stats);
    std::unique_ptr<DuplexConnection> framedConnection =
        std::make_unique<FramedDuplexConnection>(
            std::move(connection), inlineExecutor());
    std::unique_ptr<RequestHandler> requestHandler =
        std::make_unique<ServerRequestHandler>();

    auto rs = StandardReactiveSocket::fromServerConnection(
        eventBase_,
        std::move(framedConnection),
        std::move(requestHandler),
        stats);

    rs->onClosed([ this, rs = rs.get() ](const folly::exception_wrapper& ex) {
      removeSocket(*rs);
    });

    reactiveSockets_.push_back(std::move(rs));
  }

  void removeSocket(ReactiveSocket& socket) {
    if (!shuttingDown_) {
      reactiveSockets_.erase(std::remove_if(
          reactiveSockets_.begin(),
          reactiveSockets_.end(),
          [&socket](std::unique_ptr<StandardReactiveSocket>& vecSocket) {
            return vecSocket.get() == &socket;
          }));
    }
  }

  virtual void acceptError(const std::exception& ex) noexcept override {
    std::cout << "AcceptError " << ex.what() << std::endl;
  }

  // unused
  // void shutdown() {
  //   shuttingDown_ = true;
  //   reactiveSockets_.clear();
  // }

 private:
  class ServerRequestHandler : public DefaultRequestHandler {
   public:
    ServerRequestHandler() {
      marbles_ = parseMarbles(FLAGS_test_file);
    }

    void handleRequestStream(
        Payload request,
        StreamId streamId,
        const std::shared_ptr<Subscriber<Payload>>&
            response) noexcept override {
      LOG(INFO) << "handleRequestStream " << request;
      std::string data = request.data->moveToFbString().toStdString();
      std::string metadata = request.metadata->moveToFbString().toStdString();
      auto it = marbles_.streamMarbles.find(std::make_pair(data, metadata));
      if (it == marbles_.streamMarbles.end()) {
        LOG(ERROR) << "No Handler found for the [data: " << data
                   << ", metadata:" << metadata << "]";
      } else {
        auto marbleProcessor =
            std::make_shared<tck::MarbleProcessor>(it->second, response);
        auto subscription =
            std::make_shared<ServerSubscription>(response, marbleProcessor, 0);
        response->onSubscribe(subscription);
        std::thread mpThread([marbleProcessor] { marbleProcessor->run(); });
        mpThread.detach();
      }
    }

    void handleRequestResponse(
        Payload request,
        StreamId streamId,
        const std::shared_ptr<Subscriber<Payload>>&
            response) noexcept override {
      LOG(INFO) << "handleRequestResponse " << request;
      std::string data = request.data->moveToFbString().toStdString();
      std::string metadata = request.metadata->moveToFbString().toStdString();
      auto it = marbles_.reqRespMarbles.find(std::make_pair(data, metadata));
      if (it == marbles_.reqRespMarbles.end()) {
        LOG(ERROR) << "No Handler found for the [data: " << data
                   << ", metadata:" << metadata << "]";
      } else {
        auto marbleProcessor =
            std::make_shared<tck::MarbleProcessor>(it->second, response);
        auto subscription =
            std::make_shared<ServerSubscription>(response, marbleProcessor, 0);
        response->onSubscribe(subscription);
        std::thread mpThread([marbleProcessor] { marbleProcessor->run(); });
        mpThread.detach();
      }
    }

    void handleFireAndForgetRequest(
        Payload request,
        StreamId streamId) noexcept override {
      LOG(INFO) << "handleFireAndForgetRequest " << request.moveDataToString();
    }

    void handleMetadataPush(
        std::unique_ptr<folly::IOBuf> request) noexcept override {
      LOG(INFO) << "handleMetadataPush " << request->moveToFbString();
    }

    std::shared_ptr<StreamState> handleSetupPayload(
        ReactiveSocket&,
        ConnectionSetupPayload request) noexcept override {
      LOG(INFO) << "handleSetupPayload " << request;
      return nullptr;
    }

   private:
    MarbleStore marbles_;
  };

  std::vector<std::unique_ptr<StandardReactiveSocket>> reactiveSockets_;
  EventBase& eventBase_;
  bool shuttingDown_{false};
};
}

[[noreturn]] static void signal_handler(int signal) {
  LOG(INFO) << "Terminating program after receiving signal " << signal;
  exit(signal);
}

int main(int argc, char* argv[]) {
#ifdef OSS
  google::ParseCommandLineFlags(&argc, &argv, true);
#else
  gflags::ParseCommandLineFlags(&argc, &argv, true);
#endif

  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();
  FLAGS_logtostderr = true;

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  folly::ScopedEventBaseThread evbt;

  Callback callback(*evbt.getEventBase());

  auto serverSocket = AsyncServerSocket::newSocket(evbt.getEventBase());

  evbt.getEventBase()->runInEventBaseThreadAndWait(
      [&evbt, &serverSocket, &callback]() {
        evbt.getEventBase()->setName("RsSockEvb");
        LOG(INFO) << "Binding to " << FLAGS_ip << ":" << FLAGS_port;
        folly::SocketAddress addr(FLAGS_ip, FLAGS_port, true);
        serverSocket->setReusePortEnabled(true);
        serverSocket->bind(addr);
        serverSocket->addAcceptCallback(&callback, evbt.getEventBase());
        serverSocket->listen(10);
        serverSocket->startAccepting();
        LOG(INFO) << "Server listening on " << addr.describe();
      });

  while (true)
    ;
}

// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <gmock/gmock.h>
#include <reactive-streams/utilities/SmartPointers.h>
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

// Simple ResumeIdentificationToken to StreamState map
class StreamStateMap {
public:

  std::shared_ptr<StreamState> findExistingStreamState(
    const ResumeIdentificationToken& token) {
    auto it = map_.find(token);
    std::shared_ptr<StreamState> result;

    if (it != map_.end()) {
      result = (*it).second;
    }

    return result;
  }

  std::shared_ptr<StreamState> newStreamState(
    const ResumeIdentificationToken& token) {
    std::shared_ptr<StreamState> streamState = std::make_shared<StreamState>();

    map_[token] = streamState;
    return streamState;
  }

protected:
  template<typename T, std::size_t N>
  class TokenHash {
  public:
    std::size_t operator()(std::array<T, N> const &arr) const {
      std::size_t sum = 0;

      for (auto &&i : arr) {
        sum += std::hash<T>()(i);
      }

      return sum;
    }
  };

private:
  std::unordered_map<
      ResumeIdentificationToken,
      std::shared_ptr<StreamState>,
      TokenHash<std::uint8_t, 16>> map_;
};

class ServerSubscription : public SubscriptionBase {
 public:
  explicit ServerSubscription(std::shared_ptr<Subscriber<Payload>> response)
      : response_(std::move(response)) {}

  ~ServerSubscription(){};

  // Subscription methods
  void requestImpl(size_t n) override {
    response_.onNext(Payload("from server"));
    response_.onNext(Payload("from server2"));
    //response_.onComplete();
    //    response_.onError(std::runtime_error("XXX"));
  }

  void cancelImpl() override {}

 private:
  SubscriberPtr<Subscriber<Payload>> response_;
};

std::ostream& operator<<(std::ostream& os, const ResumeIdentificationToken & token) {
  std::stringstream str;

  for (std::uint8_t byte : token) {
    str << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
  }

  return os << str.str();
}

class ServerRequestHandler : public DefaultRequestHandler {
 public:
  explicit ServerRequestHandler(std::shared_ptr<StreamStateMap> streamStateMap)
      : streamStateMap_(streamStateMap) {}

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
    std::shared_ptr<StreamState> streamState = streamStateMap_->newStreamState(request.token);

    LOG(INFO) << "ServerRequestHandler.handleSetupPayload " << request
      << " setup token <" << request.token << "> " << streamState.get();

    return streamState;
  }

  std::shared_ptr<StreamState> handleResume(
      const ResumeIdentificationToken& token) override {
    std::shared_ptr<StreamState> streamState = streamStateMap_->findExistingStreamState(token);

    LOG(INFO) << "ServerRequestHandler.handleResume resume token <" << token << "> "
      << streamState.get();

    return streamState;
  }

  void handleCleanResume(StreamId streamId, ErrorStream& errorStream) override {
    LOG(INFO) << "clean resume streamId " << streamId;
  }

  void handleDirtyResume(StreamId streamId, ErrorStream& errorStream) override {
    LOG(INFO) << "dirty resume streamId " << streamId;
  }

 private:
  std::shared_ptr<StreamStateMap> streamStateMap_;
};

class Callback : public AsyncServerSocket::AcceptCallback {
 public:
  Callback(EventBase& eventBase, Stats& stats)
      : streamStateMap_(std::make_shared<StreamStateMap>()),
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
        folly::make_unique<ServerRequestHandler>(streamStateMap_);

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
  std::shared_ptr<StreamStateMap> streamStateMap_;
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

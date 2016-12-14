// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/io/async/EventBaseManager.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <gmock/gmock.h>
#include <uuid/uuid.h>
#include "src/NullRequestHandler.h"
#include "src/ReactiveSocket.h"
#include "src/folly/FollyKeepaliveTimer.h"
#include "src/framed/FramedDuplexConnection.h"
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
}

class ClientResumeStatus : public ClientResumeStatusCallback {
public:
  void onResumeOk() override {
    std::cout << "resumed OK" << "\n";
  }

  void onConnectionError(folly::exception_wrapper ex) override {
    std::cout << "resume connection error " << ex.what() << "\n";
  }
};

class GuidGenerator {
public:
  // method to generate a UUID for the Resume Token
  void static generateAndFill(ResumeIdentificationToken& token) {
      ::uuid_t uuid;

      ::memset(&uuid, 0, sizeof(uuid));
      ::uuid_generate(uuid);
      ::memcpy(token.data(), &uuid, std::min(sizeof(uuid), token.size()));
  }
};

std::ostream& operator<<(std::ostream& os, const ResumeIdentificationToken & token) {
  std::stringstream str;

  for (std::uint8_t byte : token) {
    str << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
  }

  return os << str.str();
}

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;

  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  ScopedEventBaseThread eventBaseThread;

  std::unique_ptr<ReactiveSocket> reactiveSocket;
  Callback callback;
  StatsPrinter stats;

  ResumeIdentificationToken token;
  GuidGenerator::generateAndFill(token);

  LOG(INFO) << "Token <" << token << ">";

  eventBaseThread.getEventBase()->runInEventBaseThreadAndWait([&]() {
    folly::SocketAddress addr(FLAGS_host, FLAGS_port, true);

    folly::AsyncSocket::UniquePtr socket(
        new folly::AsyncSocket(eventBaseThread.getEventBase()));
    socket->connect(&callback, addr);

    std::cout << "attempting connection to " << addr.describe() << "\n";

    std::unique_ptr<DuplexConnection> connection =
        folly::make_unique<TcpDuplexConnection>(std::move(socket), stats);
    std::unique_ptr<DuplexConnection> framedConnection =
        folly::make_unique<FramedDuplexConnection>(std::move(connection));
    std::unique_ptr<RequestHandler> requestHandler =
        folly::make_unique<DefaultRequestHandler>();

    reactiveSocket = ReactiveSocket::fromClientConnection(
        std::move(framedConnection),
        std::move(requestHandler),
        ConnectionSetupPayload(
            "text/plain", "text/plain", Payload("meta", "data")),
        stats,
        folly::make_unique<FollyKeepaliveTimer>(
            *eventBaseThread.getEventBase(), std::chrono::milliseconds(5000)),
        token);

    reactiveSocket->requestSubscription(
        Payload("from client"), std::make_shared<PrintSubscriber>());
  });

  std::string input;
  std::getline(std::cin, input);

  eventBaseThread.getEventBase()->runInEventBaseThreadAndWait([&]() {
    folly::SocketAddress addr(FLAGS_host, FLAGS_port, true);

    folly::AsyncSocket::UniquePtr socketResume(
        new folly::AsyncSocket(eventBaseThread.getEventBase()));
    socketResume->connect(&callback, addr);

    std::unique_ptr<DuplexConnection> connectionResume =
        folly::make_unique<TcpDuplexConnection>(std::move(socketResume), stats);
    std::unique_ptr<DuplexConnection> framedConnectionResume =
        folly::make_unique<FramedDuplexConnection>(std::move(connectionResume));
    std::unique_ptr<ClientResumeStatus> statusCallback =
        folly::make_unique<ClientResumeStatus>();

    reactiveSocket->tryClientResume(
        token,
        std::move(framedConnectionResume),
        std::move(statusCallback));
  });

  std::getline(std::cin, input);

  // TODO why need to shutdown in eventbase?
  eventBaseThread.getEventBase()->runInEventBaseThreadAndWait(
      [&reactiveSocket]() { reactiveSocket.reset(nullptr); });

  return 0;
}

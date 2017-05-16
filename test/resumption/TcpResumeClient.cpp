// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/io/async/EventBaseManager.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>
#include <gmock/gmock.h>

#include "src/internal/ClientResumeStatusCallback.h"
#include "src/framing/FrameTransport.h"
#include "src/temporary_home/NullRequestHandler.h"
#include "test/deprecated/ReactiveSocket.h"
#include "src/internal/FollyKeepaliveTimer.h"
#include "src/framing/FramedDuplexConnection.h"
#include "src/transports/tcp/TcpDuplexConnection.h"
#include "test/test_utils/PrintSubscriber.h"
#include "test/test_utils/StatsPrinter.h"

using namespace ::testing;
using namespace ::reactivesocket;
using namespace ::folly;
using namespace yarpl;

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

class ResumeCallback : public ClientResumeStatusCallback {
  void onResumeOk() noexcept override {
    LOG(INFO) << "resumeOk";
  }

  // Called when an ERROR frame with CONNECTION_ERROR is received during
  // resuming operation
  void onResumeError(folly::exception_wrapper ex) noexcept override {
    LOG(INFO) << "resumeError: " << ex.what();
  }

  // Called when the resume operation was interrupted due to network
  // the application code may try to resume again.
  void onConnectionError(folly::exception_wrapper ex) noexcept override {
    LOG(INFO) << "connectionError: " << ex.what();
  }
};
}

class ClientRequestHandler : public DefaultRequestHandler {
 public:
  void onSubscriptionPaused(
      const yarpl::Reference<yarpl::flowable::Subscription>& subscription) noexcept override {
    LOG(INFO) << "subscription paused " << &subscription;
  }

  void onSubscriptionResumed(
      const yarpl::Reference<yarpl::flowable::Subscription>& subscription) noexcept override {
    LOG(INFO) << "subscription resumed " << &subscription;
  }

  void onSubscriberPaused(const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
                              subscriber) noexcept override {
    LOG(INFO) << "subscriber paused " << &subscriber;
  }

  void onSubscriberResumed(const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
                               subscriber) noexcept override {
    LOG(INFO) << "subscriber resumed " << &subscriber;
  }
};

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;

#ifdef OSS
  google::ParseCommandLineFlags(&argc, &argv, true);
#else
  gflags::ParseCommandLineFlags(&argc, &argv, true);
#endif

  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  ScopedEventBaseThread eventBaseThread;

  std::unique_ptr<ReactiveSocket> reactiveSocket;
  Callback callback;
  auto stats = std::make_shared<StatsPrinter>();

  auto token = ResumeIdentificationToken::generateNew();

  eventBaseThread.getEventBase()->runInEventBaseThreadAndWait([&]() {
    folly::SocketAddress addr(FLAGS_host, FLAGS_port, true);

    folly::AsyncSocket::UniquePtr socket(
        new folly::AsyncSocket(eventBaseThread.getEventBase()));
    socket->connect(&callback, addr);

    LOG(INFO) << "attempting connection to " << addr.describe();

    std::unique_ptr<DuplexConnection> connection =
        std::make_unique<TcpDuplexConnection>(
            std::move(socket), inlineExecutor(), stats);
    std::unique_ptr<DuplexConnection> framedConnection =
        std::make_unique<FramedDuplexConnection>(
            std::move(connection), *eventBaseThread.getEventBase());
    std::unique_ptr<RequestHandler> requestHandler =
        std::make_unique<ClientRequestHandler>();

    reactiveSocket = ReactiveSocket::disconnectedClient(
        *eventBaseThread.getEventBase(),
        std::move(requestHandler),
        stats,
        std::make_unique<FollyKeepaliveTimer>(
            *eventBaseThread.getEventBase(), std::chrono::seconds(10)));

    reactiveSocket->onConnected([]() { LOG(INFO) << "socket connected"; });
    reactiveSocket->onDisconnected([](const folly::exception_wrapper& ex) {
      LOG(INFO) << "socket disconnect: " << ex.what();
    });
    reactiveSocket->onClosed([](const folly::exception_wrapper& ex) {
      LOG(INFO) << "socket closed: " << ex.what();
    });

    LOG(INFO) << "requestStream:";
    reactiveSocket->requestStream(
        Payload("from client"), make_ref<PrintSubscriber>());

    LOG(INFO) << "connecting RS ...";
    reactiveSocket->clientConnect(
        std::make_shared<FrameTransport>(std::move(framedConnection)),
        ConnectionSetupPayload(
            "text/plain", "text/plain", Payload("meta", "data"), true, token));
  });

  std::string input;
  std::getline(std::cin, input);

  eventBaseThread.getEventBase()->runInEventBaseThreadAndWait([&]() {
    LOG(INFO) << "disconnecting RS ...";
    reactiveSocket->disconnect();
    LOG(INFO) << "requestStream:";
    reactiveSocket->requestStream(
        Payload("from client2"), make_ref<PrintSubscriber>());
  });

  std::getline(std::cin, input);

  eventBaseThread.getEventBase()->runInEventBaseThreadAndWait([&]() {
    folly::SocketAddress addr(FLAGS_host, FLAGS_port, true);

    LOG(INFO) << "new TCP connection ...";
    folly::AsyncSocket::UniquePtr socketResume(
        new folly::AsyncSocket(eventBaseThread.getEventBase()));
    socketResume->connect(&callback, addr);

    std::unique_ptr<DuplexConnection> connectionResume =
        std::make_unique<TcpDuplexConnection>(
            std::move(socketResume), inlineExecutor(), stats);
    std::unique_ptr<DuplexConnection> framedConnectionResume =
        std::make_unique<FramedDuplexConnection>(
            std::move(connectionResume), inlineExecutor());

    LOG(INFO) << "try resume ...";
    reactiveSocket->tryClientResume(
        token,
        std::make_shared<FrameTransport>(std::move(framedConnectionResume)),
        std::make_unique<ResumeCallback>());
  });

  std::getline(std::cin, input);

  // TODO why need to shutdown in eventbase?
  eventBaseThread.getEventBase()->runInEventBaseThreadAndWait(
      [&reactiveSocket]() {
        LOG(INFO) << "releasing RS";
        reactiveSocket.reset(nullptr);
      });

  return 0;
}

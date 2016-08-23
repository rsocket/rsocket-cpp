// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include "TestFileParser.h"
#include "TestInterpreter.h"
#include "TestSuite.h"
#include "src/NullRequestHandler.h"
#include "src/ReactiveSocket.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");
DEFINE_string(test_file, "", "host to connect to");

using namespace reactivesocket;
using namespace reactivesocket::tck;

namespace {

class SocketConnectCallback : public folly::AsyncSocket::ConnectCallback {
 public:
  void connectSuccess() noexcept override {
    LOG(INFO) << "tcp connection successful";
    {
      std::unique_lock<std::mutex> lock(connectionMutex_);
      connected_ = true;
    }
    connectedCV_.notify_one();
  }

  void connectErr(const folly::AsyncSocketException& ex) noexcept override {
    LOG(ERROR) << "unable to connect to TCP server: " << ex.what();
    throw ex;
  }

  void waitToConnect() {
    std::unique_lock<std::mutex> lock(connectionMutex_);
    if (!connectedCV_.wait_for(
            lock, std::chrono::seconds(5), [&] { return connected_.load(); })) {
      throw new std::runtime_error("unable to connect to tcp server");
    }
  }

 private:
  std::mutex connectionMutex_;
  std::condition_variable connectedCV_;
  std::atomic<bool> connected_{false};
};

} // namespace

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  CHECK(!FLAGS_test_file.empty())
      << "please provide test file (txt) via test_file parameter";

  LOG(INFO) << "Parsing test file " << FLAGS_test_file << " ...";

  TestFileParser testFileParser(FLAGS_test_file);
  TestSuite testSuite = testFileParser.parse();

  folly::ScopedEventBaseThread evbt;

  folly::AsyncSocket::UniquePtr socket;
  std::unique_ptr<SocketConnectCallback> callback;
  std::unique_ptr<ReactiveSocket> reactiveSocket;

  evbt.getEventBase()->runInEventBaseThreadAndWait([&]() {
    socket.reset(new folly::AsyncSocket(evbt.getEventBase()));
    callback = folly::make_unique<SocketConnectCallback>();
    folly::SocketAddress addr(FLAGS_host, FLAGS_port, true);

    LOG(INFO) << "attempting connection to " << addr.describe();
    socket->connect(callback.get(), addr);
  });

  callback->waitToConnect();

  evbt.getEventBase()->runInEventBaseThreadAndWait([&]() {
    std::unique_ptr<DuplexConnection> connection =
        folly::make_unique<TcpDuplexConnection>(std::move(socket));
    std::unique_ptr<DuplexConnection> framedConnection =
        folly::make_unique<FramedDuplexConnection>(std::move(connection));
    std::unique_ptr<RequestHandler> requestHandler =
        folly::make_unique<DefaultRequestHandler>();

    reactiveSocket = ReactiveSocket::fromClientConnection(
        std::move(framedConnection), std::move(requestHandler));
  });

  LOG(INFO) << "Test file parsed. Starting executing tests...";

  int passed = 0;
  for (const auto& test : testSuite.tests()) {
    TestInterpreter interpreter(test, *reactiveSocket, *evbt.getEventBase());
    bool passing = interpreter.run();
    if (passing) {
      ++passed;
    }
  }

  LOG(INFO) << "Tests execution DONE. " << passed << " out of "
            << testSuite.tests().size() << " tests passed.";

  evbt.getEventBase()->runInEventBaseThreadAndWait([&]() {
    reactiveSocket.reset();
    callback.reset();
    socket.reset();
  });
  return 0;
}
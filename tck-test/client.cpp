// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/String.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>

#include "src/NullRequestHandler.h"
#include "src/ReactiveSocket.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

#include "tck-test/TestFileParser.h"
#include "tck-test/TestInterpreter.h"

DEFINE_string(ip, "127.0.0.1", "IP to connect to");
DEFINE_int32(port, 9898, "port to connect to");
DEFINE_string(test_file, "../tck-test/clienttest.txt", "test file to run");
DEFINE_string(
    tests,
    "all",
    "Comma separated names of tests to run. By default run all tests");
DEFINE_int32(timeout, 5, "timeout (in secs) for connecting to the server");

using namespace reactivesocket;
using namespace reactivesocket::tck;

namespace {

class SocketConnectCallback : public folly::AsyncSocket::ConnectCallback {
 public:
  void connectSuccess() noexcept override {
    LOG(INFO) << "TCP connection successful";
    {
      std::unique_lock<std::mutex> lock(connectionMutex_);
      connected_ = true;
    }
    connectedCV_.notify_one();
  }

  void connectErr(const folly::AsyncSocketException& ex) noexcept override {
    LOG(FATAL) << "Unable to connect to TCP server: " << ex.what();
  }

  void waitToConnect() {
    std::unique_lock<std::mutex> lock(connectionMutex_);
    if (!connectedCV_.wait_for(lock, std::chrono::seconds(FLAGS_timeout), [&] {
          return connected_.load();
        })) {
      throw std::runtime_error("Unable to connect to tcp server");
    }
  }

 private:
  std::mutex connectionMutex_;
  std::condition_variable connectedCV_;
  std::atomic<bool> connected_{false};
};

} // namespace

int main(int argc, char* argv[]) {
#ifdef OSS
  google::ParseCommandLineFlags(&argc, &argv, true);
#else
  gflags::ParseCommandLineFlags(&argc, &argv, true);
#endif

  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();
  FLAGS_logtostderr = true;

  CHECK(!FLAGS_test_file.empty())
      << "please provide test file (txt) via test_file parameter";

  LOG(INFO) << "Parsing test file " << FLAGS_test_file;

  folly::ScopedEventBaseThread evbt;

  folly::AsyncSocket::UniquePtr socket;
  std::unique_ptr<SocketConnectCallback> callback;
  std::unique_ptr<ReactiveSocket> reactiveSocket;

  evbt.getEventBase()->runInEventBaseThreadAndWait([&]() {
    socket.reset(new folly::AsyncSocket(evbt.getEventBase()));
    callback = std::make_unique<SocketConnectCallback>();
    LOG(INFO) << "Connecting to " << FLAGS_ip << ":" << FLAGS_port;
    folly::SocketAddress addr(FLAGS_ip, FLAGS_port, true);
    socket->connect(callback.get(), addr);
    LOG(INFO) << "Connected to " << addr.describe();
  });

  callback->waitToConnect();

  evbt.getEventBase()->runInEventBaseThreadAndWait([&]() {
    evbt.getEventBase()->setName("RsSockEvb");
    std::unique_ptr<DuplexConnection> connection =
        std::make_unique<TcpDuplexConnection>(
            std::move(socket), inlineExecutor());
    std::unique_ptr<DuplexConnection> framedConnection =
        std::make_unique<FramedDuplexConnection>(
            std::move(connection), inlineExecutor());
    std::unique_ptr<RequestHandler> requestHandler =
        std::make_unique<DefaultRequestHandler>();

    reactiveSocket = ReactiveSocket::fromClientConnection(
        inlineExecutor(),
        std::move(framedConnection),
        std::move(requestHandler));
  });

  TestFileParser testFileParser(FLAGS_test_file);
  TestSuite testSuite = testFileParser.parse();
  LOG(INFO) << "Test file parsed. Executing " << testSuite.tests().size()
            << " tests.";

  int ran = 0, passed = 0;
  std::vector<std::string> testsToRun;
  folly::split(",", FLAGS_tests, testsToRun);
  for (const auto& test : testSuite.tests()) {
    if (FLAGS_tests == "all" ||
        std::find(testsToRun.begin(), testsToRun.end(), test.name()) !=
            testsToRun.end()) {
      ++ran;
      TestInterpreter interpreter(test, *reactiveSocket);
      bool passing = interpreter.run(evbt.getEventBase());
      if (passing) {
        ++passed;
      }
    }
  }

  LOG(INFO) << "Tests execution DONE. " << passed << " out of " << ran
            << " tests passed.";

  evbt.getEventBase()->runInEventBaseThreadAndWait([&]() {
    reactiveSocket.reset();
    callback.reset();
    socket.reset();
  });
  return !(passed == ran);
}

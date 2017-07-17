// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/SocketAddress.h>
#include <folly/String.h>
#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"

#include "tck-test/TestFileParser.h"
#include "tck-test/TestInterpreter.h"

#include "rsocket/transports/tcp/TcpConnectionFactory.h"

DEFINE_string(ip, "127.0.0.1", "IP to connect to");
DEFINE_int32(port, 9898, "port to connect to");
DEFINE_string(test_file, "../tck-test/clienttest.txt", "test file to run");
DEFINE_string(
    tests,
    "all",
    "Comma separated names of tests to run. By default run all tests");
DEFINE_int32(timeout, 5, "timeout (in secs) for connecting to the server");

using namespace rsocket;
using namespace rsocket::tck;

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  CHECK(!FLAGS_test_file.empty())
      << "please provide test file (txt) via test_file parameter";

  LOG(INFO) << "Parsing test file " << FLAGS_test_file;

  folly::SocketAddress address;
  address.setFromHostPort(FLAGS_ip, FLAGS_port);

  LOG(INFO) << "Creating client to connect to " << address.describe();

  // create a client which can then make connections below
  auto rsf = RSocket::createClient(
      std::make_unique<TcpConnectionFactory>(std::move(address)));

  // connect and wait for connection
  auto rs = rsf->connect().get();

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
      TestInterpreter interpreter(test, rs.get());
      bool passing = interpreter.run();
      if (passing) {
        ++passed;
      }
    }
  }

  LOG(INFO) << "Tests execution DONE. " << passed << " out of " << ran
            << " tests passed.";

  return !(passed == ran);
}

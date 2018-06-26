// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <folly/Memory.h>
#include <folly/SocketAddress.h>
#include <folly/String.h>
#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"

#include "rsocket/tck-test/TestFileParser.h"
#include "rsocket/tck-test/TestInterpreter.h"

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
      TestInterpreter interpreter(test, std::move(address));
      bool passing = interpreter.run();
      ++ran;
      if (passing) {
        ++passed;
      }
    }
  }

  LOG(INFO) << "Tests execution DONE. " << passed << " out of " << ran
            << " tests passed.";

  return !(passed == ran);
}

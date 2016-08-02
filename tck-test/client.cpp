// Copyright 2004-present Facebook. All Rights Reserved.

#include <gflags/gflags.h>
#include <glog/logging.h>
#include "TestFileParser.h"
#include "TestInterpreter.h"
#include "TestSuite.h"

DEFINE_string(
    test_server,
    "localhost:9898",
    "ip:port of the test server to runt the client against");
DEFINE_string(test_file, "", "host to connect to");

using namespace reactivesocket::tck;

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  CHECK(!FLAGS_test_file.empty())
      << "please provide test file (txt) via test_file parameter";

  LOG(INFO) << "Parsing test file " << FLAGS_test_file << "...";

  TestFileParser testFileParser(FLAGS_test_file);
  TestSuite testSuite = testFileParser.parse();

  LOG(INFO) << "Test file parsed. Starting executing tests...";

  for (const auto& test : testSuite.tests()) {
    TestInterpreter interpreter(test);
    interpreter.run();
  }

  LOG(INFO) << "All tests executed.";
  return 0;
}
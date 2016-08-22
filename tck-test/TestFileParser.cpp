// Copyright 2004-present Facebook. All Rights Reserved.

#include "TestFileParser.h"
#include <folly/String.h>
#include <glog/logging.h>

namespace reactivesocket {
namespace tck {

TestFileParser::TestFileParser(const std::string& fileName)
    : input_(fileName) {}

TestSuite TestFileParser::parse() {
  currentLine_ = 0;
  std::string newCommand;
  while (std::getline(input_, newCommand)) {
    parseCommand(newCommand);
    ++currentLine_;
  }

  addCurrentTest();
  return std::move(testSuite_);
}

void TestFileParser::parseCommand(const std::string& command) {
  if (command.empty()) {
    // ignore empty lines
    return;
  }

  // test delimiter
  if (command == "!") {
    addCurrentTest();
    return;
  }

  std::vector<std::string> parameters;
  folly::split("%%", command, parameters, /*ignoreEmpty=*/true);

  if (parameters.size() == 2 && parameters[0] == "name") {
    currentTest_.setName(parameters[1]);
    return;
  }
  if (parameters.size() == 1 && (parameters[0] == "pass" || parameters[0] == "fail")) {
    currentTest_.setSucceeding(parameters[0] == "pass");
    return;
  }

  TestCommand newCommand(std::move(parameters));
  if (!newCommand.valid()) {
    LOG(ERROR) << "invalid command on line " << currentLine_ << ": " << command;
    throw std::runtime_error("unknown command in the test");
  } else {
    currentTest_.addCommand(std::move(newCommand));
  }
}

void TestFileParser::addCurrentTest() {
  if (!currentTest_.empty()) {
    testSuite_.addTest(std::move(currentTest_));
    DCHECK(currentTest_.empty());
  }
}

} // tck
} // reactive socket

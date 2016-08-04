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

  return std::move(testSuite_);
}

void TestFileParser::parseCommand(const std::string& command) {
  if (command.empty()) {
    // ignore empty lines
    return;
  }

  if (command == "!") {
    // test delimiter
    if (currentTest_.empty()) {
      // no test defined
    } else {
      testSuite_.addTest(std::move(currentTest_));
    }
    return;
  }

  std::vector<std::string> parameters;
  folly::split("%%", command, parameters, /*ignoreEmpty=*/true);

  if (parameters.size() == 2 && parameters[0] == "name") {
    currentTest_.setName(parameters[1]);
    return;
  }

  TestCommand newCommand(std::move(parameters));
  if (!newCommand.valid()) {
    LOG(ERROR) << "invalid command on line " << currentLine_ << ": " << command
               << " (ignoring)";
  } else {
    currentTest_.addCommand(std::move(newCommand));
  }
}

} // tck
} // reactive socket

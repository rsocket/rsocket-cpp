// Copyright 2004-present Facebook. All Rights Reserved.

#include "tck-test/TestFileParser.h"

#include <folly/String.h>
#include <glog/logging.h>

namespace rsocket {
namespace tck {

TestFileParser::TestFileParser(const std::string& fileName) : input_(fileName) {
  if (!input_.good()) {
    LOG(FATAL) << "Could not read from file " << fileName;
  }
}

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
  if (command == "!" || command == "EOF") {
    addCurrentTest();
    return;
  }

  std::vector<std::string> parameters;
  folly::split("%%", command, parameters, /*ignoreEmpty=*/true);

  if (parameters.size() == 2 && parameters[0] == "name") {
    currentTest_.setName(parameters[1]);
    currentTest_.setResumption(false);
    return;
  }

  TestCommand newCommand(std::move(parameters));
  if (!newCommand.valid()) {
    LOG(ERROR) << "invalid command on line " << currentLine_ << ": " << command;
    throw std::runtime_error("unknown command in the test");
  } else {
    // if test contain resumption related command.
    if ("disconnect" == newCommand.name() || "resume" == newCommand.name()) {
      currentTest_.setResumption(true);
    }

    currentTest_.addCommand(std::move(newCommand));
  }
}

void TestFileParser::addCurrentTest() {
  if (!currentTest_.empty()) {
    testSuite_.addTest(std::move(currentTest_));
    DCHECK(currentTest_.empty());
  }
}

} // namespace tck
} // namespace rsocket

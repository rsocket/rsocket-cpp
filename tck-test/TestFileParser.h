// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <fstream>
#include "TestSuite.h"

namespace reactivesocket {
namespace tck {

class TestFileParser {
 public:
  explicit TestFileParser(const std::string& fileName);

  TestSuite parse();

 private:
  void parseCommand(const std::string& command);
  void addCurrentTest();

  std::ifstream input_;
  int currentLine_;

  TestSuite testSuite_;
  Test currentTest_;
};

} // tck
} // reactivesocket

// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <string>
#include <vector>

namespace reactivesocket {
namespace tck {

class TestCommand {
 public:
  explicit TestCommand(std::vector<std::string> params)
      : params_(std::move(params)) {}

  const std::string& name() const {
    return params_[0];
  }

  template <typename T>
  T as() const {
    return T(*this);
  }

  const std::vector<std::string>& params() const {
    return params_;
  }

  bool valid() const;

 private:
  std::vector<std::string> params_;
};

class Test {
 public:
  const std::string& name() const {
    return name_;
  }

  void setName(const std::string& name) {
    name_ = name;
  }

  void addCommand(TestCommand command);

  const std::vector<TestCommand>& commands() const {
    return commands_;
  }

  bool empty() const {
    return commands_.empty();
  }

 private:
  std::string name_;
  std::vector<TestCommand> commands_;
};

class TestSuite {
 public:
  void addTest(Test test) {
    tests_.push_back(std::move(test));
  }

  const std::vector<Test>& tests() const {
    return tests_;
  }

 private:
  std::vector<Test> tests_;
};

} // tck
} // reactive socket

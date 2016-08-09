// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Conv.h>
#include "TestSuite.h"

namespace reactivesocket {
namespace tck {

class TypedTestCommand {
 public:
  explicit TypedTestCommand(const TestCommand& command) : command_(command) {}

 protected:
  const TestCommand& command_;
};

class SubscribeCommand : public TypedTestCommand {
 public:
  using TypedTestCommand::TypedTestCommand;

  const std::string& type() const {
    return command_.params().at(1);
  }
  const std::string& id() const {
    return command_.params().at(2);
  }
  const std::string& data() const {
    return command_.params().at(3);
  }
  const std::string& metadata() const {
    return command_.params().at(4);
  }
};

class RequestCommand : public TypedTestCommand {
 public:
  using TypedTestCommand::TypedTestCommand;

  int n() const {
    return folly::to<int>(command_.params().at(1));
  }
  const std::string& id() const {
    return command_.params().at(2);
  }
};

class CancelCommand : public TypedTestCommand {
 public:
  using TypedTestCommand::TypedTestCommand;

  const std::string& id() const {
    return command_.params().at(1);
  }
};

class AwaitCommand : public TypedTestCommand {
 public:
  using TypedTestCommand::TypedTestCommand;

  const std::string& type() const {
    return command_.params().at(1);
  }
  const std::string& id() const {
    return command_.params().at(2);
  }
  int numElements() const {
    return folly::to<int>(command_.params().at(3));
  }
};

class AssertCommand : public TypedTestCommand {
 public:
  using TypedTestCommand::TypedTestCommand;

  const std::string& assertion() const {
    return command_.params().at(1);
  }
  const std::string& id() const {
    return command_.params().at(2);
  }
};

} // tck
} // reactive socket

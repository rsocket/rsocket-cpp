// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Conv.h>
#include <folly/String.h>
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

  bool isRequestResponseType() const {
    return type() == "rr";
  }
  bool isRequestStreamType() const {
    return type() == "rs";
  }
  bool isRequestSubscriptionType() const {
    return type() == "sub";
  }
  bool isFireAndForgetType() const {
    return type() == "fnf";
  }

  const std::string& id() const {
    return command_.params().at(2);
  }
  const std::string& payloadData() const {
    return command_.params().at(3);
  }
  const std::string& payloadMetadata() const {
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
  bool isTerminalType() const {
    return type() == "terminal";
  }
  bool isAtLeastType() const {
    return type() == "atLeast";
  }
  bool isNoEventsType() const {
    return type() == "no_events";
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
  bool isNoErrorAssert() const {
    return assertion() == "no_error";
  }
  bool isErrorAssert() const {
    return assertion() == "error";
  }
  bool isReceivedAssert() const {
    return assertion() == "received";
  }
  bool isReceivedNAssert() const {
    return assertion() == "received_n";
  }
  bool isReceivedAtLeastAssert() const {
    return assertion() == "received_at_least";
  }
  bool isCompletedAssert() const {
    return assertion() == "completed";
  }
  bool isNotCompletedAssert() const {
    return assertion() == "no_completed";
  }
  bool isCanceledAssert() const {
    return assertion() == "canceled";
  }

  const std::string& id() const {
    return command_.params().at(2);
  }

  std::vector<std::pair<std::string, std::string>> values() const {
    const auto& valuesStr = command_.params().at(3);
    std::vector<std::string> items;
    folly::split("&&", valuesStr, items);

    std::vector<std::string> components;
    std::vector<std::pair<std::string, std::string>> values;
    for (const auto& item : items) {
      components.clear();
      folly::split(",", item, components);
      if (components.size() == 2) {
        values.emplace_back(std::make_pair(components[0], components[1]));
      } else {
        LOG(ERROR) << "wrong item in values string: " << item;
      }
    }
    return values;
  }

  size_t valueCount() const {
    return folly::to<size_t>(command_.params().at(3));
  }
};

} // tck
} // reactive socket

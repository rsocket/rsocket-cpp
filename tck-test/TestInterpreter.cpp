// Copyright 2004-present Facebook. All Rights Reserved.

#include "TestInterpreter.h"
#include <glog/logging.h>
#include "TypedCommands.h"

namespace reactivesocket {
namespace tck {

TestInterpreter::TestInterpreter(const Test& test) : test_(test) {
  DCHECK(!test.empty());
}

void TestInterpreter::run() {
  LOG(INFO) << "executing test " << test_.name() << "...";

  for (const auto& command : test_.commands()) {
    if (command.name() == "subscribe") {
      // TODO
      auto subscribe = command.as<SubscribeCommand>();
      (void)subscribe;
    } else if (command.name() == "request") {
      auto request = command.as<RequestCommand>();
      // TODO
      (void)request;
    } else if (command.name() == "await") {
      auto await = command.as<AwaitCommand>();
      // TODO
      (void)await;
    } else if (command.name() == "cancel") {
      auto cancel = command.as<CancelCommand>();
      // TODO
      (void)cancel;
    } else if (command.name() == "assert") {
      auto assert = command.as<AssertCommand>();
      // TODO
      (void)assert;
    } else {
      LOG(ERROR) << "unknown command " << command.name();
      break;
    }
  }

  LOG(INFO) << "test " << test_.name() << " done";
}

} // tck
} // reactive socket

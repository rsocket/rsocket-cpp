// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/tck-test/TestSuite.h"

#include <glog/logging.h>

namespace rsocket {
namespace tck {

bool TestCommand::valid() const {
  // there has to be a name to the test and at least 1 param
  return params_.size() >= 1;
}

void Test::addCommand(TestCommand command) {
  CHECK(command.valid());
  commands_.push_back(std::move(command));
}

} // tck
} // reactivesocket

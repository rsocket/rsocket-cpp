// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

} // namespace tck
} // namespace rsocket

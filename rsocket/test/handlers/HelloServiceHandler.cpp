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

#include "rsocket/test/handlers/HelloServiceHandler.h"
#include "rsocket/test/handlers/HelloStreamRequestHandler.h"

namespace rsocket {
namespace tests {

folly::Expected<RSocketConnectionParams, RSocketException>
HelloServiceHandler::onNewSetup(const SetupParameters&) {
  return RSocketConnectionParams(
      std::make_shared<rsocket::tests::HelloStreamRequestHandler>(),
      RSocketStats::noop(),
      connectionEvents_);
}

void HelloServiceHandler::onNewRSocketState(
    std::shared_ptr<RSocketServerState> state,
    ResumeIdentificationToken token) {
  store_.lock()->insert({token, std::move(state)});
}

folly::Expected<std::shared_ptr<RSocketServerState>, RSocketException>
HelloServiceHandler::onResume(ResumeIdentificationToken token) {
  auto itr = store_->find(token);
  CHECK(itr != store_->end());
  return itr->second;
}

} // namespace tests
} // namespace rsocket

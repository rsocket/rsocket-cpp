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

#include <gtest/gtest.h>

#include <folly/io/async/EventBase.h>

#include "rsocket/RSocketConnectionEvents.h"
#include "rsocket/RSocketResponder.h"
#include "rsocket/RSocketStats.h"
#include "rsocket/internal/ConnectionSet.h"
#include "rsocket/internal/KeepaliveTimer.h"
#include "rsocket/statemachine/RSocketStateMachine.h"

using namespace rsocket;

namespace {

std::shared_ptr<RSocketStateMachine> makeStateMachine(folly::EventBase* evb) {
  return std::make_shared<RSocketStateMachine>(
      std::make_shared<RSocketResponder>(),
      std::make_unique<KeepaliveTimer>(std::chrono::seconds{10}, *evb),
      RSocketMode::SERVER,
      RSocketStats::noop(),
      std::make_shared<RSocketConnectionEvents>(),
      ResumeManager::makeEmpty(),
      nullptr /* coldResumeHandler */
  );
}
} // namespace

TEST(ConnectionSet, ImmediateDtor) {
  ConnectionSet set;
}

TEST(ConnectionSet, CloseViaMachine) {
  folly::EventBase evb;
  auto machine = makeStateMachine(&evb);

  ConnectionSet set;
  set.insert(machine, &evb);
  machine->registerCloseCallback(&set);

  machine->close({}, StreamCompletionSignal::CANCEL);
}

TEST(ConnectionSet, CloseViaSetDtor) {
  folly::EventBase evb;
  auto machine = makeStateMachine(&evb);

  ConnectionSet set;
  set.insert(machine, &evb);
  machine->registerCloseCallback(&set);
}

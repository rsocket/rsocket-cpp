// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>

#include <folly/io/async/EventBase.h>

#include "rsocket/RSocketConnectionEvents.h"
#include "rsocket/RSocketResponder.h"
#include "rsocket/RSocketStats.h"
#include "rsocket/internal/ConnectionSet.h"
#include "rsocket/internal/FollyKeepaliveTimer.h"
#include "rsocket/statemachine/RSocketStateMachine.h"

using namespace rsocket;
using namespace testing;

namespace {

std::shared_ptr<RSocketStateMachine> makeStateMachine(folly::EventBase* evb) {
  return std::make_shared<RSocketStateMachine>(
      std::make_shared<RSocketResponder>(),
      std::make_unique<FollyKeepaliveTimer>(*evb, std::chrono::seconds{10}),
      RSocketMode::SERVER,
      RSocketStats::noop(),
      std::make_shared<RSocketConnectionEvents>(),
      nullptr /* resumeManager */,
      nullptr /* coldResumeHandler */
      );
}
}

TEST(ConnectionSet, ImmediateDtor) {
  ConnectionSet set;
}

TEST(ConnectionSet, CloseViaMachine) {
  folly::EventBase evb;
  auto machine = makeStateMachine(&evb);

  auto set = std::make_shared<ConnectionSet>();
  set->insert(machine, &evb);
  machine->registerSet(set);

  machine->close({}, StreamCompletionSignal::CANCEL);
}

TEST(ConnectionSet, CloseViaSetDtor) {
  folly::EventBase evb;
  auto machine = makeStateMachine(&evb);

  auto set = std::make_shared<ConnectionSet>();
  set->insert(machine, &evb);
  machine->registerSet(set);
}

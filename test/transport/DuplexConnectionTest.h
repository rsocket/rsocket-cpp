// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/EventBase.h>

#include "src/DuplexConnection.h"

namespace rsocket {
namespace tests {

void makeMultipleSetInputGetOutputCalls(
    std::unique_ptr<rsocket::DuplexConnection> serverConnection,
    folly::EventBase* serverEvb,
    std::unique_ptr<rsocket::DuplexConnection> clientConnection,
    folly::EventBase* clientEvb);

void verifyInputAndOutputIsUntied(
    std::unique_ptr<rsocket::DuplexConnection> serverConnection,
    folly::EventBase* serverEvb,
    std::unique_ptr<rsocket::DuplexConnection> clientConnection,
    folly::EventBase* clientEvb);

void verifyClosingInputAndOutputDoesntCloseConnection(
    std::unique_ptr<rsocket::DuplexConnection> serverConnection,
    folly::EventBase* serverEvb,
    std::unique_ptr<rsocket::DuplexConnection> clientConnection,
    folly::EventBase* clientEvb);

} // namespace tests
} // namespace rsocket

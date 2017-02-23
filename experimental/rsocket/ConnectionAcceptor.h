// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/EventBase.h>
#include "src/DuplexConnection.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace rsocket {

/**
 * Common interface for a server that accepts connections and turns them into
 * DuplexConnection.
 *
 * Built-in implementations can be found in rsocket/transports/, such as
 * rsocket/transports/TcpServerConnectionAcceptor.h
 */
class ConnectionAcceptor {
 public:
  virtual ~ConnectionAcceptor() = default;
  virtual void start(
      std::function<void(std::unique_ptr<DuplexConnection>, EventBase&)>
          onAccept) = 0;
};
}
// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/EventBase.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include "src/DuplexConnection.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace rsocket {

using OnConnect = std::function<void(std::unique_ptr<DuplexConnection>)>;

class ConnectionFactory {
 public:
  virtual ~ConnectionFactory() = default;
  virtual void connect(
      OnConnect onConnect,
      ScopedEventBaseThread& eventBaseThread) = 0;
};
}
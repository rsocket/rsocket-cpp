// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/DuplexConnection.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace folly {
class EventBase;
}

namespace rsocket {

using OnConnect =
    std::function<void(std::unique_ptr<DuplexConnection>, EventBase&)>;

class ConnectionFactory {
 public:
  ConnectionFactory() = default;
  virtual ~ConnectionFactory() = default;
  ConnectionFactory(const ConnectionFactory&) = delete; // copy
  ConnectionFactory(ConnectionFactory&&) = delete; // move
  ConnectionFactory& operator=(const ConnectionFactory&) = delete; // copy
  ConnectionFactory& operator=(ConnectionFactory&&) = delete; // move

  virtual void connect(OnConnect onConnect) = 0;
};
}
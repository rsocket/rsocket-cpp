// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/EventBase.h>
#include "src/DuplexConnection.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace rsocket {

using OnAccept =
    std::function<void(std::unique_ptr<DuplexConnection>, EventBase&)>;

class ServerConnectionAcceptor {
 public:
  virtual ~ServerConnectionAcceptor() = default;
  virtual void start(OnAccept onAccept) = 0;
};
}
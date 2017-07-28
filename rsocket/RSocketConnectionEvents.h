// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

namespace folly {
class exception_wrapper;
}

namespace rsocket {

class RSocketConnectionEvents {
 public:
  virtual ~RSocketConnectionEvents() = default;

  virtual void onConnected() {}
  virtual void onDisconnected(const folly::exception_wrapper&) {}
  virtual void onClosed(const folly::exception_wrapper&) {}
};
}

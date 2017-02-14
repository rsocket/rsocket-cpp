// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/StandardReactiveSocket.h"

using namespace ::reactivesocket;

namespace rsocket {

/**
 * Represents a new connection RESUME request from a client.
 */
class ConnectionResumeRequest {
 public:
  ConnectionResumeRequest() = default;
  virtual ~ConnectionResumeRequest() = default;
  ConnectionResumeRequest(const ConnectionResumeRequest&) = delete; // copy
  ConnectionResumeRequest(ConnectionResumeRequest&&) = delete; // move
  ConnectionResumeRequest& operator=(const ConnectionResumeRequest&) =
      delete; // copy
  ConnectionResumeRequest& operator=(ConnectionResumeRequest&&) =
      delete; // move

 private:
};
}
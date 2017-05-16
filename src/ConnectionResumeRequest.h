// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

namespace rsocket {

/**
 * Represents a new connection RESUME request from a client.
 *
 * Is passed to the RSocketServer resume callback for acceptance or rejection.
 */
class ConnectionResumeRequest {
 public:
  ConnectionResumeRequest(const ConnectionResumeRequest&) = delete; // copy
  ConnectionResumeRequest(ConnectionResumeRequest&&) = delete; // move
  ConnectionResumeRequest& operator=(const ConnectionResumeRequest&) =
      delete; // copy
  ConnectionResumeRequest& operator=(ConnectionResumeRequest&&) =
      delete; // move

  // TODO still just a placeholder, not implemented yet

 private:
};
}

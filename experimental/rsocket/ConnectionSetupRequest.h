// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/StandardReactiveSocket.h"

using namespace ::reactivesocket;

namespace rsocket {

/**
 * Represents a new connection SETUP request from a client.
 *
 * Is passed to the RSocketServer setup callback for acceptance or rejection.
 *
 * This provides access to the SETUP Data/Metadata, MimeTypes, and other such information
 * to allow conditional connection handling.
 */
class ConnectionSetupRequest {
 public:
  ConnectionSetupRequest(ConnectionSetupPayload setupPayload);
  virtual ~ConnectionSetupRequest() = default;
  ConnectionSetupRequest(const ConnectionSetupRequest&) = delete; // copy
  ConnectionSetupRequest(ConnectionSetupRequest&&) = delete; // move
  ConnectionSetupRequest& operator=(const ConnectionSetupRequest&) =
      delete; // copy
  ConnectionSetupRequest& operator=(ConnectionSetupRequest&&) = delete; // move

  std::string& getMetadataMimeType();
  std::string& getDataMimeType();
  Payload& getPayload();
  bool clientRequestsResumability();
  ResumeIdentificationToken& getResumeIdentificationToken();
  bool willHonorLease();

 private:
  ConnectionSetupPayload setupPayload_;
};
}
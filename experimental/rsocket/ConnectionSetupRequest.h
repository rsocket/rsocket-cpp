// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/ReactiveSocket.h"

namespace rsocket {

/**
 * Represents a new connection SETUP request from a client.
 *
 * Is passed to the RSocketServer setup callback for acceptance or rejection.
 *
 * This provides access to the SETUP Data/Metadata, MimeTypes, and other such
 * information
 * to allow conditional connection handling.
 */
class ConnectionSetupRequest {
 public:
  explicit ConnectionSetupRequest(
      reactivesocket::ConnectionSetupPayload setupPayload);
  ConnectionSetupRequest(const ConnectionSetupRequest&) = delete; // copy
  ConnectionSetupRequest(ConnectionSetupRequest&&) = default; // move
  ConnectionSetupRequest& operator=(const ConnectionSetupRequest&) =
      delete; // copy
  ConnectionSetupRequest& operator=(ConnectionSetupRequest&&) = default; // move

  const std::string& getMetadataMimeType() const;
  const std::string& getDataMimeType() const;
  const reactivesocket::Payload& getPayload() const;
  bool clientRequestsResumability() const;
  const reactivesocket::ResumeIdentificationToken&
  getResumeIdentificationToken() const;
  bool willHonorLease() const;

 private:
  reactivesocket::ConnectionSetupPayload setupPayload_;
};
}

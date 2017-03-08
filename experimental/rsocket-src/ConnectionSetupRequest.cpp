// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/ConnectionSetupRequest.h"

using namespace reactivesocket;

namespace rsocket {

ConnectionSetupRequest::ConnectionSetupRequest(
    ConnectionSetupPayload setupPayload)
    : setupPayload_(std::move(setupPayload)) {}

const std::string& ConnectionSetupRequest::getMetadataMimeType() const {
  return setupPayload_.metadataMimeType;
}
const std::string& ConnectionSetupRequest::getDataMimeType() const {
  return setupPayload_.dataMimeType;
}
const Payload& ConnectionSetupRequest::getPayload() const {
  return setupPayload_.payload;
}
bool ConnectionSetupRequest::clientRequestsResumability() const {
  return setupPayload_.resumable;
}
const ResumeIdentificationToken&
ConnectionSetupRequest::getResumeIdentificationToken() const {
  return setupPayload_.token;
}
bool ConnectionSetupRequest::willHonorLease() const {
  // TODO not implemented yet in ConnectionSetupPayload
  return false;
}
}

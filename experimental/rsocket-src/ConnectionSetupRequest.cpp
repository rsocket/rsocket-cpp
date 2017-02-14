// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/ConnectionSetupRequest.h"

using namespace ::reactivesocket;

namespace rsocket {

ConnectionSetupRequest::ConnectionSetupRequest(
    ConnectionSetupPayload setupPayload)
    : setupPayload_(std::move(setupPayload)) {}

std::string& ConnectionSetupRequest::getMetadataMimeType() {
  return setupPayload_.metadataMimeType;
}
std::string& ConnectionSetupRequest::getDataMimeType() {
  return setupPayload_.dataMimeType;
}
Payload& ConnectionSetupRequest::getPayload() {
  return setupPayload_.payload;
}
bool ConnectionSetupRequest::clientRequestsResumability() {
  return setupPayload_.resumable;
}
ResumeIdentificationToken&
ConnectionSetupRequest::getResumeIdentificationToken() {
  return setupPayload_.token;
}
bool ConnectionSetupRequest::willHonorLease() {
  // TODO not implemented yet in ConnectionSetupPayload
  return false;
}
}
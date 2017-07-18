// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>

namespace rsocket {

// Thrown when an ERROR frame with CONNECTION_ERROR is received during
// resuming operation.
// TODO: in this case we should get the DuplexConnection back to
// create a new instance of RS with it
class ResumptionException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Thrown when the resume operation was interrupted due to network
// the application code may try to resume again.
class ConnectionException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

class ClientResumeStatusCallback {
 public:
  virtual ~ClientResumeStatusCallback() = default;

  // Called when a RESUME_OK frame is received during resuming operation
  virtual void onResumeOk() noexcept = 0;

  // The exception could be one of ResumptionException or ConnectionException
  virtual void onResumeError(folly::exception_wrapper ex) noexcept = 0;
};

} // reactivesocket

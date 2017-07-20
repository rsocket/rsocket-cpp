// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>

#include "rsocket/RSocketException.h"

namespace rsocket {
class ClientResumeStatusCallback {
 public:
  virtual ~ClientResumeStatusCallback() = default;

  // Called when a RESUME_OK frame is received during resuming operation
  virtual void onResumeOk() noexcept = 0;

  // The exception could be one of ResumptionException or ConnectionException
  virtual void onResumeError(folly::exception_wrapper ex) noexcept = 0;
};

} // reactivesocket

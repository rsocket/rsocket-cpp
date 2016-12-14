// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>

namespace reactivesocket {

class ClientResumeStatusHandler {
public:
  virtual ~ClientResumeStatusHandler() = default;

  // Called when a RESUME_OK frame is received during resuming operation
  virtual void onResumeOk() = 0;

  // Called when an ERROR frame with CONNECTION_ERROR is received during resuming operation
  virtual void onResumeError(folly::exception_wrapper ex) = 0;
};

class DefaultClientResumeStatusHandler : public ClientResumeStatusHandler {
public:
  virtual void onResumeOk() {}
  virtual void onResumeError(folly::exception_wrapper ex) {}
};
}

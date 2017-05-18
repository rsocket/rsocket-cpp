// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <string>

namespace rsocket {

/*
 * Error Codes from
 * https://github.com/ReactiveSocket/reactivesocket/blob/master/Protocol.md#error-codes
 */
class RSocketError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;

  /**
   * Get the error code for inclusion in an RSocket ERROR frame as per
   * https://github.com/ReactiveSocket/reactivesocket/blob/master/Protocol.md#error-codes
   * @return
   */
  virtual int getErrorCode() = 0;
};

/**
 * Error Code: INVALID_SETUP 0x00000001
 */
class InvalidSetupError : public RSocketError {
 public:
  using RSocketError::RSocketError;

  int getErrorCode() override {
    return 0x00000001;
  }

  const char* what() const noexcept override {
    return "INVALID_SETUP";
  }
};

/**
 * Error Code: UNSUPPORTED_SETUP 0x00000002
 */
class UnsupportedSetupError : public RSocketError {
 public:
  using RSocketError::RSocketError;

  int getErrorCode() override {
    return 0x00000002;
  }

  const char* what() const noexcept override {
    return "UNSUPPORTED_SETUP";
  }
};

/**
 * Error Code: REJECTED_SETUP 0x00000003
 */
class RejectedSetupError : public RSocketError {
 public:
  using RSocketError::RSocketError;

  int getErrorCode() override {
    return 0x00000003;
  }

  const char* what() const noexcept override {
    return "REJECTED_SETUP";
  }
};

/**
 * Error Code: REJECTED_RESUME 0x00000004
 */
class RejectedResumeError : public RSocketError {
 public:
  using RSocketError::RSocketError;

  int getErrorCode() override {
    return 0x00000004;
  }

  const char* what() const noexcept override {
    return "REJECTED_RESUME";
  }
};

/**
 * Error Code: CONNECTION_ERROR 0x00000101
 */
class ConnectionError : public RSocketError {
 public:
  using RSocketError::RSocketError;

  int getErrorCode() override {
    return 0x00000101;
  }

  const char* what() const noexcept override {
    return "CONNECTION_ERROR";
  }
};

/**
* Error Code: CONNECTION_CLOSE 0x00000102
*/
class ConnectionCloseError : public RSocketError {
 public:
  using RSocketError::RSocketError;

  int getErrorCode() override {
    return 0x00000102;
  }

  const char* what() const noexcept override {
    return "CONNECTION_CLOSE";
  }
};
}

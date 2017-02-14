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
  explicit RSocketError(std::string s) : std::runtime_error(std::move(s)) {}
  explicit RSocketError(const char* s) : std::runtime_error(s) {}
  /**
   * Get the error code for inclusion in an RSocket ERROR frame as per
   * https://github.com/ReactiveSocket/reactivesocket/blob/master/Protocol.md#error-codes
   * @return
   */
  virtual int getErrorCode() = 0;
  /**
   * Get the string name of the error as per
   * https://github.com/ReactiveSocket/reactivesocket/blob/master/Protocol.md#error-codes
   * @return
   */
  virtual const std::string getErrorString() = 0;
};

/**
 * Error Code: INVALID_SETUP 0x00000001
 */
class InvalidSetupError : public RSocketError {
 public:
  explicit InvalidSetupError(const std::string& s) : RSocketError(s) {}
  explicit InvalidSetupError(const char* s) : RSocketError(s) {}

  int getErrorCode() override {
    return 0x00000001;
  }

  const std::string getErrorString() override {
    return std::string("INVALID_SETUP");
  }
};

/**
 * Error Code: UNSUPPORTED_SETUP 0x00000002
 */
class UnsupportedSetupError : public RSocketError {
 public:
  explicit UnsupportedSetupError(const std::string& s) : RSocketError(s) {}
  explicit UnsupportedSetupError(const char* s) : RSocketError(s) {}

  int getErrorCode() override {
    return 0x00000002;
  }

  const std::string getErrorString() override {
    return std::string("UNSUPPORTED_SETUP");
  }
};

/**
 * Error Code: REJECTED_SETUP 0x00000003
 */
class RejectedSetupError : public RSocketError {
 public:
  explicit RejectedSetupError(const std::string& s) : RSocketError(s) {}
  explicit RejectedSetupError(const char* s) : RSocketError(s) {}

  int getErrorCode() override {
    return 0x00000003;
  }

  const std::string getErrorString() override {
    return std::string("REJECTED_SETUP");
  }
};

/**
 * Error Code: REJECTED_RESUME 0x00000004
 */
class RejectedResumeError : public RSocketError {
 public:
  explicit RejectedResumeError(const std::string& s) : RSocketError(s) {}
  explicit RejectedResumeError(const char* s) : RSocketError(s) {}

  int getErrorCode() override {
    return 0x00000004;
  }

  const std::string getErrorString() override {
    return std::string("REJECTED_RESUME");
  }
};

/**
 * Error Code: CONNECTION_ERROR 0x00000101
 */
class ConnectionError : public RSocketError {
 public:
  explicit ConnectionError(const std::string& s) : RSocketError(s) {}
  explicit ConnectionError(const char* s) : RSocketError(s) {}

  int getErrorCode() override {
    return 0x00000101;
  }

  const std::string getErrorString() override {
    return std::string("CONNECTION_ERROR");
  }
};

/**
* Error Code: CONNECTION_CLOSE 0x00000102
*/
class ConnectionCloseError : public RSocketError {
 public:
  explicit ConnectionCloseError(const std::string& s) : RSocketError(s) {}
  explicit ConnectionCloseError(const char* s) : RSocketError(s) {}

  int getErrorCode() override {
    return 0x00000102;
  }

  const std::string getErrorString() override {
    return std::string("CONNECTION_CLOSE");
  }
};
}

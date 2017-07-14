// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/framing/ErrorCode.h"

#include <ostream>

namespace rsocket {

std::ostream& operator<<(std::ostream& os, ErrorCode errorCode) {
  switch (errorCode) {
    case ErrorCode::RESERVED:
      return os << "RESERVED";
    case ErrorCode::INVALID_SETUP:
      return os << "INVALID_SETUP";
    case ErrorCode::UNSUPPORTED_SETUP:
      return os << "UNSUPPORTED_SETUP";
    case ErrorCode::REJECTED_SETUP:
      return os << "REJECTED_SETUP";
    case ErrorCode::REJECTED_RESUME:
      return os << "REJECTED_RESUME";
    case ErrorCode::CONNECTION_ERROR:
      return os << "CONNECTION_ERROR";
    case ErrorCode::APPLICATION_ERROR:
      return os << "APPLICATION_ERROR";
    case ErrorCode::REJECTED:
      return os << "REJECTED";
    case ErrorCode::CANCELED:
      return os << "CANCELED";
    case ErrorCode::INVALID:
      return os << "INVALID";
  }
  return os << "ErrorCode[" << static_cast<uint32_t>(errorCode) << "]";
}
}

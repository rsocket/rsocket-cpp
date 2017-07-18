// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/framing/FrameType.h"

#include <glog/logging.h>

#include <ostream>

namespace rsocket {

std::ostream& operator<<(std::ostream& os, FrameType type) {
  switch (type) {
    case FrameType::RESERVED:
      return os << "RESERVED";
    case FrameType::SETUP:
      return os << "SETUP";
    case FrameType::LEASE:
      return os << "LEASE";
    case FrameType::KEEPALIVE:
      return os << "KEEPALIVE";
    case FrameType::REQUEST_RESPONSE:
      return os << "REQUEST_RESPONSE";
    case FrameType::REQUEST_FNF:
      return os << "REQUEST_FNF";
    case FrameType::REQUEST_STREAM:
      return os << "REQUEST_STREAM";
    case FrameType::REQUEST_CHANNEL:
      return os << "REQUEST_CHANNEL";
    case FrameType::REQUEST_N:
      return os << "REQUEST_N";
    case FrameType::CANCEL:
      return os << "CANCEL";
    case FrameType::PAYLOAD:
      return os << "PAYLOAD";
    case FrameType::ERROR:
      return os << "ERROR";
    case FrameType::METADATA_PUSH:
      return os << "METADATA_PUSH";
    case FrameType::RESUME:
      return os << "RESUME";
    case FrameType::RESUME_OK:
      return os << "RESUME_OK";
    case FrameType::EXT:
      return os << "EXT";
    default:
      break;
  }
  return os << "Unknown FrameType[" << static_cast<int>(type) << "]";
}
}

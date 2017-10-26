// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/framing/FrameType.h"

#include <ostream>

#include <glog/logging.h>

namespace rsocket {

constexpr folly::StringPiece kUnknown{"UNKNOWN_FRAME_TYPE"};

folly::StringPiece toString(FrameType type) {
  switch (type) {
    case FrameType::RESERVED:
      return "RESERVED";
    case FrameType::SETUP:
      return "SETUP";
    case FrameType::LEASE:
      return "LEASE";
    case FrameType::KEEPALIVE:
      return "KEEPALIVE";
    case FrameType::REQUEST_RESPONSE:
      return "REQUEST_RESPONSE";
    case FrameType::REQUEST_FNF:
      return "REQUEST_FNF";
    case FrameType::REQUEST_STREAM:
      return "REQUEST_STREAM";
    case FrameType::REQUEST_CHANNEL:
      return "REQUEST_CHANNEL";
    case FrameType::REQUEST_N:
      return "REQUEST_N";
    case FrameType::CANCEL:
      return "CANCEL";
    case FrameType::PAYLOAD:
      return "PAYLOAD";
    case FrameType::ERROR:
      return "ERROR";
    case FrameType::METADATA_PUSH:
      return "METADATA_PUSH";
    case FrameType::RESUME:
      return "RESUME";
    case FrameType::RESUME_OK:
      return "RESUME_OK";
    case FrameType::EXT:
      return "EXT";
    default:
      DLOG(FATAL) << "Unknown frame type";
      return kUnknown;
  }
}

std::ostream& operator<<(std::ostream& os, FrameType type) {
  auto const str = toString(type);
  if (str == kUnknown) {
    return os << "Unknown FrameType[" << static_cast<int>(type) << "]";
  }
  return os << str;
}
}

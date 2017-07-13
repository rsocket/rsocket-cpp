// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/framing/FrameSerializer_v0_1.h"

#include <folly/io/Cursor.h>

namespace rsocket {

constexpr const ProtocolVersion FrameSerializerV0_1::Version;
constexpr const size_t FrameSerializerV0_1::kMinBytesNeededForAutodetection;

ProtocolVersion FrameSerializerV0_1::protocolVersion() {
  return Version;
}

ProtocolVersion FrameSerializerV0_1::detectProtocolVersion(
    const folly::IOBuf& firstFrame,
    size_t skipBytes) {
  // SETUP frame
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |     Frame Type = SETUP        |0|M|L|S|       Flags           |
  //  +-------------------------------+-+-+-+-+-----------------------+
  //  |                          Stream ID = 0                        |
  //  +-------------------------------+-------------------------------+
  //  |     Major Version             |         Minor Version         |
  //  +-------------------------------+-------------------------------+
  //                                 ...
  //  +-------------------------------+-------------------------------+

  // RESUME frame
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |     Frame Type = RESUME       |             Flags             |
  //  +-------------------------------+-------------------------------+
  //  |                          Stream ID = 0                        |
  //  +-------------------------------+-------------------------------+
  //  |                                                               |
  //  |                    Resume Identification Token                |
  //  |                                                               |
  //  |                                                               |
  //  +-------------------------------+-------------------------------+
  //  |                       Resume Position                         |
  //  |                                                               |
  //  +-------------------------------+-------------------------------+

  folly::io::Cursor cur(&firstFrame);
  try {
    cur.skip(skipBytes);

    auto frameType = cur.readBE<uint16_t>();
    cur.skip(sizeof(uint16_t)); // flags
    auto streamId = cur.readBE<uint32_t>();

    constexpr static const auto kSETUP = 0x0001;
    constexpr static const auto kRESUME = 0x000E;

    VLOG(4) << "frameType=" << frameType << "streamId=" << streamId;

    if (frameType == kSETUP && streamId == 0) {
      auto majorVersion = cur.readBE<uint16_t>();
      auto minorVersion = cur.readBE<uint16_t>();

      VLOG(4) << "majorVersion=" << majorVersion
              << " minorVersion=" << minorVersion;

      if (majorVersion == 0 && (minorVersion == 0 || minorVersion == 1)) {
        return ProtocolVersion(majorVersion, minorVersion);
      }
    } else if (frameType == kRESUME && streamId == 0) {
      return FrameSerializerV0_1::Version;
    }
  } catch (...) {
  }
  return ProtocolVersion::Unknown;
}

} // reactivesocket

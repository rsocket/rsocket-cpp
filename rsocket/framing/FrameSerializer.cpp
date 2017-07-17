// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/framing/FrameSerializer.h"

#include <folly/Conv.h>
#include <folly/portability/GFlags.h>

#include "rsocket/framing/FrameSerializer_v0.h"
#include "rsocket/framing/FrameSerializer_v0_1.h"
#include "rsocket/framing/FrameSerializer_v1_0.h"

DEFINE_string(
    rs_use_protocol_version,
    "",
    "override for the ReactiveSocket protocol version to be used"
    " [MAJOR.MINOR].");

namespace rsocket {

constexpr const ProtocolVersion ProtocolVersion::Latest =
    FrameSerializerV1_0::Version;

ProtocolVersion FrameSerializer::getCurrentProtocolVersion() {
  if (FLAGS_rs_use_protocol_version.empty()) {
    return ProtocolVersion::Latest;
  }

  if (FLAGS_rs_use_protocol_version == "*") {
    return ProtocolVersion::Unknown;
  }

  if (FLAGS_rs_use_protocol_version.size() != 3) {
    LOG(ERROR) << "unknown protocol version " << FLAGS_rs_use_protocol_version
               << " defaulting to v" << ProtocolVersion::Latest;
    return ProtocolVersion::Latest;
  }

  return ProtocolVersion(
      folly::to<uint16_t>(FLAGS_rs_use_protocol_version[0] - '0'),
      folly::to<uint16_t>(FLAGS_rs_use_protocol_version[2] - '0'));
}

std::unique_ptr<FrameSerializer> FrameSerializer::createFrameSerializer(
    const ProtocolVersion& protocolVersion) {
  if (protocolVersion == FrameSerializerV0::Version) {
    return std::make_unique<FrameSerializerV0>();
  } else if (protocolVersion == FrameSerializerV0_1::Version) {
    return std::make_unique<FrameSerializerV0_1>();
  } else if (protocolVersion == FrameSerializerV1_0::Version) {
    return std::make_unique<FrameSerializerV1_0>();
  }

  DCHECK(protocolVersion == ProtocolVersion::Unknown);
  LOG_IF(ERROR, protocolVersion != ProtocolVersion::Unknown)
      << "unknown protocol version " << protocolVersion;
  return nullptr;
}

std::unique_ptr<FrameSerializer> FrameSerializer::createCurrentVersion() {
  return createFrameSerializer(getCurrentProtocolVersion());
}

std::unique_ptr<FrameSerializer> FrameSerializer::createAutodetectedSerializer(
    const folly::IOBuf& firstFrame) {
  auto detectedVersion = FrameSerializerV1_0::detectProtocolVersion(firstFrame);
  if (detectedVersion == ProtocolVersion::Unknown) {
    detectedVersion = FrameSerializerV0_1::detectProtocolVersion(firstFrame);
  }
  return createFrameSerializer(detectedVersion);
}

std::ostream& operator<<(std::ostream& os, const ProtocolVersion& version) {
  return os << version.major << "." << version.minor;
}

} // reactivesocket

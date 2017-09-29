// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/framing/FrameHeader.h"

#include <map>
#include <ostream>
#include <vector>

namespace rsocket {

namespace {

constexpr auto kEmpty = "0x00";
constexpr auto kMetadata = "METADATA";
constexpr auto kResumeEnable = "RESUME_ENABLE";
constexpr auto kLease = "LEASE";
constexpr auto kKeepAliveRespond = "KEEPALIVE_RESPOND";
constexpr auto kFollows = "FOLLOWS";
constexpr auto kComplete = "COMPLETE";
constexpr auto kNext = "NEXT";

std::map<FrameType, std::vector<std::pair<FrameFlags, std::string>>>
    flagToNameMap{
        {FrameType::REQUEST_N, {}},
        {FrameType::REQUEST_RESPONSE,
         {{FrameFlags::METADATA, kMetadata}, {FrameFlags::FOLLOWS, kFollows}}},
        {FrameType::REQUEST_FNF,
         {{FrameFlags::METADATA, kMetadata}, {FrameFlags::FOLLOWS, kFollows}}},
        {FrameType::METADATA_PUSH, {}},
        {FrameType::CANCEL, {}},
        {FrameType::PAYLOAD,
         {{FrameFlags::METADATA, kMetadata},
          {FrameFlags::FOLLOWS, kFollows},
          {FrameFlags::COMPLETE, kComplete},
          {FrameFlags::NEXT, kNext}}},
        {FrameType::ERROR, {{FrameFlags::METADATA, kMetadata}}},
        {FrameType::KEEPALIVE,
         {{FrameFlags::KEEPALIVE_RESPOND, kKeepAliveRespond}}},
        {FrameType::SETUP,
         {{FrameFlags::METADATA, kMetadata},
          {FrameFlags::RESUME_ENABLE, kResumeEnable},
          {FrameFlags::LEASE, kLease}}},
        {FrameType::LEASE, {{FrameFlags::METADATA, kMetadata}}},
        {FrameType::RESUME, {}},
        {FrameType::REQUEST_CHANNEL,
         {{FrameFlags::METADATA, kMetadata},
          {FrameFlags::FOLLOWS, kFollows},
          {FrameFlags::COMPLETE, kComplete}}},
        {FrameType::REQUEST_STREAM,
         {{FrameFlags::METADATA, kMetadata}, {FrameFlags::FOLLOWS, kFollows}}}};

std::ostream&
writeFlags(std::ostream& os, FrameFlags frameFlags, FrameType frameType) {
  FrameFlags foundFlags = FrameFlags::EMPTY;

  // Search the corresponding string value for each flag, insert the missing
  // ones as empty
  auto const& allowedFlags = flagToNameMap[frameType];

  std::string delimiter = "";
  for (const auto& pair : allowedFlags) {
    if (!!(frameFlags & pair.first)) {
      os << delimiter << pair.second;
      delimiter = "|";
      foundFlags |= pair.first;
    }
  }

  if (foundFlags != frameFlags) {
    os << frameFlags;
  } else if (delimiter.empty()) {
    os << kEmpty;
  }
  return os;
}
}

std::ostream& operator<<(std::ostream& os, const FrameHeader& header) {
  os << header.type << "[";
  return writeFlags(os, header.flags, header.type)
      << ", " << header.streamId << "]";
}
}

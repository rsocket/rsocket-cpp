// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/framing/Frame.h"

#include <folly/Memory.h>
#include <folly/io/Cursor.h>
#include <map>
#include <sstream>

#include "rsocket/RSocketParameters.h"

namespace rsocket {

const uint32_t Frame_LEASE::kMaxTtl;
const uint32_t Frame_LEASE::kMaxNumRequests;
const uint32_t Frame_SETUP::kMaxKeepaliveTime;
const uint32_t Frame_SETUP::kMaxLifetime;

static std::ostream&
writeFlags(std::ostream& os, FrameFlags frameFlags, FrameType frameType) {
  constexpr const char* kEmpty = "0x00";
  constexpr const char* kMetadata = "METADATA";
  constexpr const char* kResumeEnable = "RESUME_ENABLE";
  constexpr const char* kLease = "LEASE";
  constexpr const char* kKeepAliveRespond = "KEEPALIVE_RESPOND";
  constexpr const char* kFollows = "FOLLOWS";
  constexpr const char* kComplete = "COMPLETE";
  constexpr const char* kNext = "NEXT";

  static std::map<FrameType, std::vector<std::pair<FrameFlags, std::string>>>
      flagToNameMap{{FrameType::REQUEST_N, {}},
                    {FrameType::REQUEST_RESPONSE,
                     {{FrameFlags::METADATA, kMetadata},
                      {FrameFlags::FOLLOWS, kFollows}}},
                    {FrameType::REQUEST_FNF,
                     {{FrameFlags::METADATA, kMetadata},
                      {FrameFlags::FOLLOWS, kFollows}}},
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
                     {{FrameFlags::METADATA, kMetadata},
                      {FrameFlags::FOLLOWS, kFollows}}}};

  FrameFlags foundFlags = FrameFlags::EMPTY;

  // Search the corresponding string value for each flag, insert the missing
  // ones as empty
  const std::vector<std::pair<FrameFlags, std::string>>& allowedFlags =
      flagToNameMap[frameType];

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

std::ostream& operator<<(std::ostream& os, const FrameHeader& header) {
  os << header.type_ << "[";
  return writeFlags(os, header.flags_, header.type_) << ", " << header.streamId_ << "]";
}

/// @}

std::ostream& operator<<(std::ostream& os, const Frame_REQUEST_Base& frame) {
  return os << frame.header_ << "(" << frame.requestN_ << ", "
            << frame.payload_;
}

std::ostream& operator<<(std::ostream& os, const Frame_REQUEST_N& frame) {
  return os << frame.header_ << "(" << frame.requestN_ << ")";
}

std::ostream& operator<<(
    std::ostream& os,
    const Frame_REQUEST_RESPONSE& frame) {
  return os << frame.header_ << ", " << frame.payload_;
}

std::ostream& operator<<(std::ostream& os, const Frame_REQUEST_FNF& frame) {
  return os << frame.header_ << ", " << frame.payload_;
}

std::ostream& operator<<(std::ostream& os, const Frame_METADATA_PUSH& frame) {
  return os << frame.header_ << ", "
            << (frame.metadata_ ? frame.metadata_->computeChainDataLength()
                                : 0);
}

std::ostream& operator<<(std::ostream& os, const Frame_CANCEL& frame) {
  return os << frame.header_;
}

Frame_PAYLOAD Frame_PAYLOAD::complete(StreamId streamId) {
  return Frame_PAYLOAD(streamId, FrameFlags::COMPLETE, Payload());
}

std::ostream& operator<<(std::ostream& os, const Frame_PAYLOAD& frame) {
  return os << frame.header_ << ", " << frame.payload_;
}

Frame_ERROR Frame_ERROR::invalidSetup(std::string message) {
  return connectionErr(ErrorCode::INVALID_SETUP, std::move(message));
}

Frame_ERROR Frame_ERROR::unsupportedSetup(std::string message) {
  return connectionErr(ErrorCode::UNSUPPORTED_SETUP, std::move(message));
}

Frame_ERROR Frame_ERROR::rejectedSetup(std::string message) {
  return connectionErr(ErrorCode::REJECTED_SETUP, std::move(message));
}

Frame_ERROR Frame_ERROR::rejectedResume(std::string message) {
  return connectionErr(ErrorCode::REJECTED_RESUME, std::move(message));
}

Frame_ERROR Frame_ERROR::connectionError(std::string message) {
  return connectionErr(ErrorCode::CONNECTION_ERROR, std::move(message));
}

Frame_ERROR Frame_ERROR::applicationError(
    StreamId stream,
    std::string message) {
  return streamErr(ErrorCode::APPLICATION_ERROR, std::move(message), stream);
}

Frame_ERROR Frame_ERROR::rejected(StreamId stream, std::string message) {
  return streamErr(ErrorCode::REJECTED, std::move(message), stream);
}

Frame_ERROR Frame_ERROR::canceled(StreamId stream, std::string message) {
  return streamErr(ErrorCode::CANCELED, std::move(message), stream);
}

Frame_ERROR Frame_ERROR::invalid(StreamId stream, std::string message) {
  return streamErr(ErrorCode::INVALID, std::move(message), stream);
}

Frame_ERROR Frame_ERROR::connectionErr(
    ErrorCode err,
    std::string message) {
  return Frame_ERROR{0, err, Payload{std::move(message)}};
}

Frame_ERROR Frame_ERROR::streamErr(
    ErrorCode err,
    std::string message,
    StreamId stream) {
  if (stream == 0) {
    throw std::invalid_argument{"Can't make stream error for stream zero"};
  }
  return Frame_ERROR{stream, err, Payload{std::move(message)}};
}

std::ostream& operator<<(std::ostream& os, const Frame_ERROR& frame) {
  return os << frame.header_ << ", " << frame.errorCode_ << ", "
            << frame.payload_;
  }

  std::ostream& operator<<(std::ostream& os, const Frame_KEEPALIVE& frame) {
    return os << frame.header_ << "(<"
              << (frame.data_ ? frame.data_->computeChainDataLength() : 0)
              << ">)";
  }

  std::ostream& operator<<(std::ostream& os, const Frame_SETUP& frame) {
    return os << frame.header_ << ", Version: " << frame.versionMajor_ << "."
        << frame.versionMinor_ << ", "
        << "Token: " << frame.token_ << ", " << frame.payload_;
  }

  void Frame_SETUP::moveToSetupPayload(SetupParameters & setupPayload) {
    setupPayload.metadataMimeType = std::move(metadataMimeType_);
    setupPayload.dataMimeType = std::move(dataMimeType_);
    setupPayload.payload = std::move(payload_);
    setupPayload.token = std::move(token_);
    setupPayload.resumable = !!(header_.flags_ & FrameFlags::RESUME_ENABLE);
    setupPayload.protocolVersion =
        ProtocolVersion(versionMajor_, versionMinor_);
  }

  std::ostream& operator<<(std::ostream& os, const Frame_LEASE& frame) {
    return os << frame.header_ << ", ("
              << (frame.metadata_ ? frame.metadata_->computeChainDataLength()
                                  : 0)
              << ")";
  }

  std::ostream& operator<<(std::ostream& os, const Frame_RESUME& frame) {
    return os << frame.header_ << ", ("
              << "token " << frame.token_ << ", @server "
              << frame.lastReceivedServerPosition_ << ", @client "
              << frame.clientPosition_ << ")";
  }

  std::ostream& operator<<(std::ostream& os, const Frame_RESUME_OK& frame) {
    return os << frame.header_ << ", (@" << frame.position_ << ")";
  }

  std::ostream& operator<<(
      std::ostream& os, const Frame_REQUEST_CHANNEL& frame) {
    return os << frame.header_ << ", " << frame.payload_;
  }

  std::ostream& operator<<(
      std::ostream& os, const Frame_REQUEST_STREAM& frame) {
    return os << frame.header_ << ", initialRequestN=" << frame.requestN_
              << ", " << frame.payload_;
  }

  StreamType getStreamType(FrameType frameType) {
    if (frameType == FrameType::REQUEST_STREAM) {
      return StreamType::STREAM;
    } else if (frameType == FrameType::REQUEST_CHANNEL) {
      return StreamType::CHANNEL;
    } else if (frameType == FrameType::REQUEST_RESPONSE) {
      return StreamType::REQUEST_RESPONSE;
    } else if (frameType == FrameType::REQUEST_FNF) {
      return StreamType::FNF;
    } else {
      LOG(FATAL) << "Unknown open stream frame : " << frameType;
    }
  }

  bool isNewStreamFrame(FrameType frameType) {
    return frameType == FrameType::REQUEST_CHANNEL ||
        frameType == FrameType::REQUEST_STREAM ||
        frameType == FrameType::REQUEST_RESPONSE ||
        frameType == FrameType::REQUEST_FNF;
  }

} // namespace rsocket

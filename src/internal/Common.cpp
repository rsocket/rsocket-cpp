// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/internal/Common.h"

#include <folly/Random.h>
#include <folly/String.h>
#include <folly/io/IOBuf.h>
#include <random>

namespace rsocket {

namespace {
constexpr const char* HEX_CHARS = {"0123456789abcdef"};
}

constexpr const ProtocolVersion ProtocolVersion::Unknown = ProtocolVersion(
    std::numeric_limits<uint16_t>::max(),
    std::numeric_limits<uint16_t>::max());

static const char* getTerminatingSignalErrorMessage(int terminatingSignal) {
  switch (static_cast<StreamCompletionSignal>(terminatingSignal)) {
    case StreamCompletionSignal::CONNECTION_END:
      return "connection closed";
    case StreamCompletionSignal::CONNECTION_ERROR:
      return "connection error";
    case StreamCompletionSignal::ERROR:
      return "socket or stream error";
    case StreamCompletionSignal::APPLICATION_ERROR:
      return "application error";
    case StreamCompletionSignal::SOCKET_CLOSED:
      return "reactive socket closed";
    case StreamCompletionSignal::UNSUPPORTED_SETUP:
      return "unsupported setup";
    case StreamCompletionSignal::REJECTED_SETUP:
      return "rejected setup";
    case StreamCompletionSignal::INVALID_SETUP:
      return "invalid setup";
    case StreamCompletionSignal::COMPLETE:
    case StreamCompletionSignal::CANCEL:
      DCHECK(false) << "throwing exception for graceful termination?";
      return "graceful termination";
    default:
      return "stream interrupted";
  }
}

std::string to_string(StreamCompletionSignal signal) {
  switch (signal) {
    case StreamCompletionSignal::COMPLETE:
      return "COMPLETE";
    case StreamCompletionSignal::CANCEL:
      return "CANCEL";
    case StreamCompletionSignal::ERROR:
      return "ERROR";
    case StreamCompletionSignal::APPLICATION_ERROR:
      return "APPLICATION_ERROR";
    case StreamCompletionSignal::INVALID_SETUP:
      return "INVALID_SETUP";
    case StreamCompletionSignal::UNSUPPORTED_SETUP:
      return "UNSUPPORTED_SETUP";
    case StreamCompletionSignal::REJECTED_SETUP:
      return "REJECTED_SETUP";
    case StreamCompletionSignal::CONNECTION_ERROR:
      return "CONNECTION_ERROR";
    case StreamCompletionSignal::CONNECTION_END:
      return "CONNECTION_END";
    case StreamCompletionSignal::SOCKET_CLOSED:
      return "SOCKET_CLOSED";
  }
  // this should be never hit because the switch is over all cases
  LOG(FATAL) << "unknown StreamCompletionSignal=" << static_cast<int>(signal);
}

std::ostream& operator<<(std::ostream& os, StreamCompletionSignal signal) {
  return os << to_string(signal);
}

StreamInterruptedException::StreamInterruptedException(int _terminatingSignal)
    : std::runtime_error(getTerminatingSignalErrorMessage(_terminatingSignal)),
      terminatingSignal(_terminatingSignal) {}

ResumeIdentificationToken::ResumeIdentificationToken() {}

ResumeIdentificationToken ResumeIdentificationToken::generateNew() {
  constexpr size_t kSize = 16;
  std::vector<uint8_t> data;
  data.reserve(kSize);
  for (size_t i = 0; i < kSize; i++) {
    data.push_back(static_cast<uint8_t>(folly::Random::rand32()));
  }
  return ResumeIdentificationToken(std::move(data));
}

void ResumeIdentificationToken::set(std::vector<uint8_t> newBits) {
  CHECK(newBits.size() <= std::numeric_limits<uint16_t>::max());
  bits_ = std::move(newBits);
}

std::ostream& operator<<(
    std::ostream& out,
    const ResumeIdentificationToken& token) {
  out << "0x";
  for (auto b : token.data()) {
    out << HEX_CHARS[(b & 0xF0) >> 4];
    out << HEX_CHARS[b & 0x0F];
  }
  return out;
}

std::string hexDump(folly::StringPiece s) {
  return folly::hexDump(s.data(), s.size());
}
} // reactivesocket

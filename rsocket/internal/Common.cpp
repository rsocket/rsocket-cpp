// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rsocket/internal/Common.h"

#include <sstream>

#include <folly/Random.h>
#include <folly/String.h>
#include <folly/io/IOBuf.h>
#include <algorithm>
#include <random>

namespace rsocket {

namespace {
constexpr const char* HEX_CHARS = {"0123456789abcdef"};
}

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

folly::StringPiece toString(StreamType t) {
  switch (t) {
    case StreamType::REQUEST_RESPONSE:
      return "REQUEST_RESPONSE";
    case StreamType::STREAM:
      return "STREAM";
    case StreamType::CHANNEL:
      return "CHANNEL";
    case StreamType::FNF:
      return "FNF";
    default:
      DCHECK(false);
      return "(invalid StreamType)";
  }
}

std::ostream& operator<<(std::ostream& os, StreamType t) {
  return os << toString(t);
}

std::ostream& operator<<(std::ostream& os, RSocketMode mode) {
  switch (mode) {
    case RSocketMode::CLIENT:
      return os << "CLIENT";
    case RSocketMode::SERVER:
      return os << "SERVER";
  }
  DLOG(FATAL) << "Invalid RSocketMode";
  return os << "INVALID_RSOCKET_MODE";
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
  return "<unknown StreamCompletionSignal>";
}

std::ostream& operator<<(std::ostream& os, StreamCompletionSignal signal) {
  return os << to_string(signal);
}

StreamInterruptedException::StreamInterruptedException(int _terminatingSignal)
    : std::runtime_error(getTerminatingSignalErrorMessage(_terminatingSignal)),
      terminatingSignal(_terminatingSignal) {}

ResumeIdentificationToken::ResumeIdentificationToken() {}

ResumeIdentificationToken::ResumeIdentificationToken(const std::string& token) {
  const auto getNibble = [&token](size_t i) {
    uint8_t nibble;
    if (token[i] >= '0' && token[i] <= '9') {
      nibble = token[i] - '0';
    } else if (token[i] >= 'a' && token[i] <= 'f') {
      nibble = token[i] - 'a' + 10;
    } else {
      throw std::invalid_argument("ResumeToken not in right format: " + token);
    }
    return nibble;
  };
  if (token.size() < 2 || token[0] != '0' || token[1] != 'x' ||
      (token.size() % 2) != 0) {
    throw std::invalid_argument("ResumeToken not in right format: " + token);
  }
  size_t i = 2;
  while (i < token.size()) {
    const uint8_t firstNibble = getNibble(i++);
    const uint8_t secondNibble = getNibble(i++);
    bits_.push_back((firstNibble << 4) | secondNibble);
  }
}

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

std::string ResumeIdentificationToken::str() const {
  std::stringstream out;
  out << *this;
  return out.str();
}

std::ostream& operator<<(
    std::ostream& out,
    const ResumeIdentificationToken& token) {
  out << "0x";
  for (const auto b : token.data()) {
    out << HEX_CHARS[(b & 0xF0) >> 4];
    out << HEX_CHARS[b & 0x0F];
  }
  return out;
}

std::string humanify(std::unique_ptr<folly::IOBuf> const& buf) {
  std::string ret;
  size_t cursor = 0;

  for (const auto range : *buf) {
    for (const unsigned char chr : range) {
      if (cursor >= 20)
        goto outer;
      ret += chr;
      cursor++;
    }
  }
outer:

  return folly::humanify(ret);
}
std::string hexDump(folly::StringPiece s) {
  return folly::hexDump(s.data(), std::min<size_t>(0xFF, s.size()));
}
} // namespace rsocket

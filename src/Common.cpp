// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/Common.h"
#include <folly/Random.h>
#include <folly/io/IOBuf.h>
#include <random>
#include "src/AbstractStreamAutomaton.h"

namespace reactivesocket {

namespace {

constexpr static const char* HEX_CHARS = {"0123456789abcdef"};
}

static const char* getTerminatingSignalErrorMessage(int terminatingSignal) {
  switch (static_cast<StreamCompletionSignal>(terminatingSignal)) {
    case StreamCompletionSignal::CONNECTION_END:
      return "connection closed";
    case StreamCompletionSignal::CONNECTION_ERROR:
      return "connection error";
    case StreamCompletionSignal::ERROR:
      return "socket error";
    case StreamCompletionSignal::SOCKET_CLOSED:
      return "reactive socket closed";
    case StreamCompletionSignal::UNSUPPORTED_SETUP:
      return "unsupported setup";
    case StreamCompletionSignal::REJECTED_SETUP:
      return "rejected setup";
    case StreamCompletionSignal::INVALID_SETUP:
      return "invalid setup";
    case StreamCompletionSignal::GRACEFUL:
      DCHECK(false) << "throwing exception for GRACEFUL termination?";
      return "graceful termination";
    default:
      return "stream interrupted";
  }
}

std::string to_string(StreamCompletionSignal signal) {
  switch (signal) {
    case StreamCompletionSignal::GRACEFUL:
      return "GRACEFUL";
    case StreamCompletionSignal::ERROR:
      return "ERROR";
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

ResumeIdentificationToken::ResumeIdentificationToken()
    : ResumeIdentificationToken(
          Data() = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}) {}

ResumeIdentificationToken ResumeIdentificationToken::generateNew() {
  // TODO: this will be replaced with a variable length bit array and the value
  // will be generated outside of reactivesocket
  // for now we will use temporary number generator
  folly::ThreadLocalPRNG rng;

  Data data;
  for (size_t i = 0; i < data.size(); i++) {
    data[i] = static_cast<Data::value_type>(folly::Random::rand32(rng));
  }
  return ResumeIdentificationToken(std::move(data));
}

ResumeIdentificationToken ResumeIdentificationToken::fromString(
    const std::string& /*str*/) {
  CHECK(false) << "not implemented";
  return ResumeIdentificationToken();
}

std::ostream& operator<<(
    std::ostream& out,
    const ResumeIdentificationToken& token) {
  for (auto b : token.data()) {
    out << HEX_CHARS[(b & 0xF0) >> 4];
    out << HEX_CHARS[b & 0x0F];
  }
  return out;
}

} // reactivesocket

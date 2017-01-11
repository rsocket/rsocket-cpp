// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/Common.h"
#include <folly/io/IOBuf.h>
#include <folly/Random.h>
#include <random>
#include "src/AbstractStreamAutomaton.h"

namespace reactivesocket {

namespace {

const char* HEX_CHARS = {"0123456789abcdef"};
}

static const char* getTerminatingSignalErrorMessage(int terminatingSignal) {
  switch ((StreamCompletionSignal)terminatingSignal) {
    case StreamCompletionSignal::CONNECTION_END:
      return "connection closed";
    case StreamCompletionSignal::CONNECTION_ERROR:
      return "connection error";
    case StreamCompletionSignal::ERROR:
      return "socket error";
    case StreamCompletionSignal::SOCKET_CLOSED:
      return "reactive socket closed";
    case StreamCompletionSignal::GRACEFUL:
      DCHECK(false) << "throwing exception for GRACEFUL termination?";
      return "graceful termination";
    default:
      return "stream interrupted";
  }
}

std::ostream& operator<<(std::ostream& os, StreamCompletionSignal signal) {
  switch (signal) {
    case StreamCompletionSignal::GRACEFUL:
      return os << "GRACEFUL";
    case StreamCompletionSignal::ERROR:
      return os << "ERROR";
    case StreamCompletionSignal::INVALID_SETUP:
      return os << "INVALID_SETUP";
    case StreamCompletionSignal::UNSUPPORTED_SETUP:
      return os << "UNSUPPORTED_SETUP";
    case StreamCompletionSignal::REJECTED_SETUP:
      return os << "REJECTED_SETUP";
    case StreamCompletionSignal::CONNECTION_ERROR:
      return os << "CONNECTION_ERROR";
    case StreamCompletionSignal::CONNECTION_END:
      return os << "CONNECTION_END";
    case StreamCompletionSignal::SOCKET_CLOSED:
      return os << "SOCKET_CLOSED";
  }
  // this should be never hit because the switch is over all cases
  LOG(FATAL) << "unknown StreamCompletionSignal=" << (int)signal;
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
    data[i] = folly::Random::rand32(rng);
  }
  return ResumeIdentificationToken(std::move(data));
}

ResumeIdentificationToken ResumeIdentificationToken::fromString(
    const std::string& /*str*/) {
  CHECK(false) << "not implemented";
  return ResumeIdentificationToken();
}

std::string ResumeIdentificationToken::toString() const {
  std::string str;
  for (auto b : bits_) {
    str += HEX_CHARS[(b & 0xF0) >> 4];
    str += HEX_CHARS[b & 0x0F];
  }
  return str;
}

} // reactivesocket

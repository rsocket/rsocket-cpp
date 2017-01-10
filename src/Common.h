// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

//
// this file includes all PUBLIC common types.
//

namespace reactivesocket {

class StandardReactiveSocket;

using ReactiveSocketCallback = std::function<void(StandardReactiveSocket&)>;
using StreamId = uint32_t;
using ResumePosition = int64_t;

/// Indicates the reason why the stream automaton received a terminal signal
/// from the connection.
enum class StreamCompletionSignal {
  GRACEFUL,
  ERROR,
  INVALID_SETUP,
  UNSUPPORTED_SETUP,
  REJECTED_SETUP,
  CONNECTION_ERROR,
  CONNECTION_END,
  SOCKET_CLOSED,
};

std::ostream& operator<<(std::ostream&, StreamCompletionSignal);

class StreamInterruptedException : public std::runtime_error {
 public:
  explicit StreamInterruptedException(int _terminatingSignal);
  int terminatingSignal;
};

class ResumeIdentificationToken {
 public:
  using Data = std::array<uint8_t, 16>;

  /// Creates an empty token.
  ResumeIdentificationToken();

  static ResumeIdentificationToken generateNew();
  static ResumeIdentificationToken fromString(const std::string& str);

  std::string toString() const;

  const Data& data() const {
    return bits_;
  }

  void set(Data newBits) {
    bits_ = std::move(newBits);
  }

  bool operator==(const ResumeIdentificationToken& right) const {
    return data() == right.data();
  }

  bool operator!=(const ResumeIdentificationToken& right) const {
    return data() != right.data();
  }

 private:
  explicit ResumeIdentificationToken(Data bits) : bits_(std::move(bits)) {}

  Data bits_;
};

class FrameSink;
// Client Side Keepalive Timer
class KeepaliveTimer {
 public:
  virtual ~KeepaliveTimer() = default;

  virtual std::chrono::milliseconds keepaliveTime() = 0;
  virtual void stop() = 0;
  virtual void start(const std::shared_ptr<FrameSink>& connection) = 0;
  virtual void keepaliveReceived() = 0;
};

} // reactive socket

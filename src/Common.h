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

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

class ReactiveSocket;

using ErrorCallback =
    std::function<void(const folly::exception_wrapper&) noexcept>;
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

enum class ReactiveSocketMode { SERVER, CLIENT };

std::string to_string(StreamCompletionSignal);
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

std::ostream& operator<<(std::ostream&, const ResumeIdentificationToken&);

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

} // reactivesocket

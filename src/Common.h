// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

//
// this file includes all PUBLIC common types.
//

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

class ReactiveSocket;
enum class FrameType : uint8_t;

std::string to_string(FrameType);
std::ostream& operator<<(std::ostream&, FrameType);

using ErrorCallback =
    std::function<void(const folly::exception_wrapper&) noexcept>;

using StreamId = uint32_t;

using ResumePosition = int64_t;
constexpr const ResumePosition kUnspecifiedResumePosition = -1;

/// Indicates the reason why the stream automaton received a terminal signal
/// from the connection.
enum class StreamCompletionSignal {
  CANCEL,
  COMPLETE,
  APPLICATION_ERROR,
  ERROR,
  INVALID_SETUP,
  UNSUPPORTED_SETUP,
  REJECTED_SETUP,
  CONNECTION_ERROR,
  CONNECTION_END,
  SOCKET_CLOSED,
};

enum class ReactiveSocketMode { SERVER, CLIENT };

enum class StreamType {
  REQUEST_RESPONSE,
  STREAM,
  CHANNEL,
  FNF,
};

std::string to_string(StreamCompletionSignal);
std::ostream& operator<<(std::ostream&, StreamCompletionSignal);

class StreamInterruptedException : public std::runtime_error {
 public:
  explicit StreamInterruptedException(int _terminatingSignal);
  int terminatingSignal;
};

class ResumeIdentificationToken {
 public:
  /// Creates an empty token.
  ResumeIdentificationToken();
  static ResumeIdentificationToken generateNew();

  const std::vector<uint8_t>& data() const {
    return bits_;
  }

  void set(std::vector<uint8_t> newBits);

  bool operator==(const ResumeIdentificationToken& right) const {
    return data() == right.data();
  }

  bool operator!=(const ResumeIdentificationToken& right) const {
    return data() != right.data();
  }

 private:
  explicit ResumeIdentificationToken(std::vector<uint8_t> bits)
      : bits_(std::move(bits)) {}

  std::vector<uint8_t> bits_;
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

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
class IOBuf;

template <typename T>
class Range;
typedef Range<const char*> StringPiece;
} // namespace folly

namespace rsocket {

constexpr std::chrono::seconds kDefaultKeepaliveInterval{5};

constexpr int64_t kMaxRequestN = std::numeric_limits<int32_t>::max();

/// A unique identifier of a stream.
using StreamId = uint32_t;

using ResumePosition = int64_t;
constexpr const ResumePosition kUnspecifiedResumePosition = -1;

std::string humanify(std::unique_ptr<folly::IOBuf> const&);
std::string hexDump(folly::StringPiece s);

/// Indicates the reason why the stream stateMachine received a terminal signal
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

enum class RSocketMode : uint8_t { SERVER, CLIENT };

std::ostream& operator<<(std::ostream&, RSocketMode);

enum class StreamType {
  REQUEST_RESPONSE,
  STREAM,
  CHANNEL,
  FNF,
};

folly::StringPiece toString(StreamType);
std::ostream& operator<<(std::ostream&, StreamType);

enum class RequestOriginator {
  LOCAL,
  REMOTE,
};

std::string to_string(StreamCompletionSignal);
std::ostream& operator<<(std::ostream&, StreamCompletionSignal);

class StreamInterruptedException : public std::runtime_error {
 public:
  explicit StreamInterruptedException(int _terminatingSignal);
  const int terminatingSignal;
};

class ResumeIdentificationToken {
 public:
  /// Creates an empty token.
  ResumeIdentificationToken();

  // The stringToken and ::str() function should complement
  // each other.  The string representation should be of the
  // format 0x44ab7cf01fd290b63140d01ee789cfb6
  explicit ResumeIdentificationToken(const std::string& stringToken);

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

  bool operator<(const ResumeIdentificationToken& right) const {
    return data() < right.data();
  }

  std::string str() const;

 private:
  explicit ResumeIdentificationToken(std::vector<uint8_t> bits)
      : bits_(std::move(bits)) {}

  std::vector<uint8_t> bits_;
};

std::ostream& operator<<(std::ostream&, const ResumeIdentificationToken&);

class FrameSink;

} // namespace rsocket

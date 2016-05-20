// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstdint>
#include <iosfwd>
#include <memory>

#include "reactivesocket-cpp/src/mixins/IntrusiveDeleter.h"

namespace reactivesocket {

class ConnectionAutomaton;
class ConnectionAutomaton;
class Frame_REQUEST_SUB;
class Frame_REQUEST_CHANNEL;
class Frame_REQUEST_N;
class Frame_CANCEL;
class Frame_RESPONSE;
class Frame_ERROR;
enum class StreamCompletionSignal;
using StreamId = uint32_t;

/// Terminates a chain of mixins that form an AbstractStreamAutomaton.
///
/// A common base class of all automatons, which introduces an interface shared
/// by all mixins.
///
/// This class, since it's a common base class of all mixins as well, stores and
/// IntrusiveDeleter's reference count, which shall be shared by all mixins that
/// perform automatic memory management.
class MixinTerminator
    // The `protected` below is intentional and makes sense.
    : protected IntrusiveDeleter {
 public:
  /// A dependent type which encapsulates all parameters needed to initialise
  /// any of the mixins and the final automata. Must be the only argument to the
  /// constructor of any mixin and must be passed by const& to mixin's Base.
  struct Parameters {
    Parameters() = default;
    Parameters(
        const std::shared_ptr<ConnectionAutomaton>& _connection,
        StreamId _streamId)
        : connection(_connection), streamId(_streamId) {}

    std::shared_ptr<ConnectionAutomaton> connection{nullptr};
    StreamId streamId{0};
  };
  explicit MixinTerminator(Parameters params)
      : connection_(std::move(params.connection)), streamId_(params.streamId) {}

 protected:
  /// @{
  /// Each mixin in the stack implements a subset of this API.
  void endStream(StreamCompletionSignal) {}

  void onNextFrame(Frame_REQUEST_SUB&) {}

  void onNextFrame(Frame_REQUEST_CHANNEL&) {}

  void onNextFrame(Frame_REQUEST_N&) {}

  void onNextFrame(Frame_CANCEL&) {}

  void onNextFrame(Frame_RESPONSE&) {}

  void onNextFrame(Frame_ERROR&) {}

  void onBadFrame() {}

  /// Logs an identification string of the automaton.
  std::ostream& logPrefix(std::ostream& os) /* = 0 */;
  /// @}

  /// A partially-owning pointer to the connection, the stream runs on.
  const std::shared_ptr<ConnectionAutomaton> connection_;
  /// An ID of the stream (within the connection) this automaton manages.
  const StreamId streamId_;
};
}

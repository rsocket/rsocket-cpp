// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <functional>
#include <iosfwd>
#include <memory>
#include "src/AbstractStreamAutomaton.h"

namespace reactivesocket {

class ConnectionAutomaton;
class RequestHandler;

///
/// A common base class of all automatons.
///
/// The instances might be destroyed on a different thread than they were
/// created.
class StreamAutomatonBase : public AbstractStreamAutomaton {
 public:
  /// A dependent type which encapsulates all parameters needed to initialise
  /// any of the mixins and the final automata. Must be the only argument to the
  /// constructor of any mixin and must be passed by const& to mixin's Base.
  struct Parameters {
    Parameters() = default;
    Parameters(
        std::shared_ptr<ConnectionAutomaton> _connection,
        StreamId _streamId)
        : connection(std::move(_connection)), streamId(_streamId) {}

    std::shared_ptr<ConnectionAutomaton> connection;
    StreamId streamId{0};
  };

  explicit StreamAutomatonBase(Parameters params)
      : connection_(std::move(params.connection)), streamId_(params.streamId) {}

  /// Logs an identification string of the automaton.
  std::ostream& logPrefix(std::ostream& os) /* = 0 */;
  /// @}

 protected:
  bool isTerminated() const {
    return isTerminated_;
  }

  void endStream(StreamCompletionSignal) override;
  void onNextFrame(Frame_REQUEST_STREAM&&) override;
  void onNextFrame(Frame_REQUEST_SUB&&) override;
  void onNextFrame(Frame_REQUEST_CHANNEL&&) override;
  void onNextFrame(Frame_REQUEST_RESPONSE&&) override;
  void onNextFrame(Frame_REQUEST_N&&) override;
  void onNextFrame(Frame_CANCEL&&) override;
  void onNextFrame(Frame_RESPONSE&&) override;
  void onNextFrame(Frame_ERROR&&) override;

  void onBadFrame() override;
  void onUnknownFrame() override;

 protected:
  /// A partially-owning pointer to the connection, the stream runs on.
  /// It is declared as const to allow only ctor to initialize it for thread
  /// safety of the dtor.
  const std::shared_ptr<ConnectionAutomaton> connection_;
  /// An ID of the stream (within the connection) this automaton manages.
  const StreamId streamId_;
  bool isTerminated_{false};
};
}

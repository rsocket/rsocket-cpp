// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <functional>
#include <iosfwd>
#include <memory>
#include "src/AbstractStreamAutomaton.h"

#include "src/RequestHandler.h"

namespace reactivesocket {

class ConnectionAutomaton;
class RequestHandlerBase;

///
/// A common base class of all automatons.
///
class StreamAutomatonBase : public AbstractStreamAutomaton {
 public:
  /// A dependent type which encapsulates all parameters needed to initialise
  /// any of the mixins and the final automata. Must be the only argument to the
  /// constructor of any mixin and must be passed by const& to mixin's Base.
  struct Parameters {
    Parameters() = default;
    Parameters(
        std::shared_ptr<ConnectionAutomaton> _connection,
        StreamId _streamId,
        std::shared_ptr<RequestHandlerBase> _handler)
        : connection(std::move(_connection)),
          streamId(_streamId),
          handler(std::move(_handler)) {}

    std::shared_ptr<ConnectionAutomaton> connection;
    StreamId streamId{0};
    std::shared_ptr<RequestHandlerBase> handler;
  };

  explicit StreamAutomatonBase(Parameters params)
      : connection_(std::move(params.connection)),
        streamId_(params.streamId),
        requestHandler_(std::move(params.handler)) {}

  /// Logs an identification string of the automaton.
  std::ostream& logPrefix(std::ostream& os) /* = 0 */;
  /// @}

  void onCleanResume() override {}
  void onDirtyResume() override {}

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

 private:
  void onUnexpectedFrame();

 protected:
  /// A partially-owning pointer to the connection, the stream runs on.
  const std::shared_ptr<ConnectionAutomaton> connection_;
  /// An ID of the stream (within the connection) this automaton manages.
  const StreamId streamId_;
  bool isTerminated_{false};
  std::shared_ptr<RequestHandlerBase> requestHandler_;
};
}

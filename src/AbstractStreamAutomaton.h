// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstddef>
#include <iosfwd>

#include "src/Payload.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

class ConnectionAutomaton;
class Frame_REQUEST_SUB;
class Frame_REQUEST_CHANNEL;
class Frame_REQUEST_N;
class Frame_CANCEL;
class Frame_RESPONSE;
class Frame_ERROR;
using StreamId = uint32_t;

/// Indicates the reason why the stream automaton received a terminal signal
/// from the connection.
enum class StreamCompletionSignal {
  GRACEFUL,
  INVALID_SETUP,
  UNSUPPORTED_SETUP,
  REJECTED_SETUP,
  CONNECTION_ERROR,
  CONNECTION_END,
};
std::ostream& operator<<(std::ostream&, StreamCompletionSignal);

/// Represents an abtract stream, which can support one of the following:
/// Channel, Subscription, Stream or RequestResponse.
///
/// The abstract class introduces no state, hence it is agnostic to the
/// threading model employed by connection and stream automata.
///
/// An important pattern to apply when implementing an automaton:
/// Whenever a state transition occurs, the automata's state should be modified
/// _before_ issuing any signals to other components, such as Subscribers,
/// Subscriptions or the connection automaton.
/// One must remember that any signal one delivers from inside of the atomaton
/// might synchronously call back into the automaton and attempt to alter its
/// state.
/// By comitting state transition before sending any signals, one ensures that
/// any signal delivered to the automaton synchronously will find automaton in a
/// consistent state.
///
/// Life cycle considerations:
/// 1. The automaton is owned by neither the user of ReactiveSocket nor the
///   instance of MultiplexingConnection.
/// 2. Lifetime of the automaton MUST be managed by its implementation.
class AbstractStreamAutomaton {
 public:
  virtual ~AbstractStreamAutomaton() = default;

  /// @{
  /// A contract exposed to the connection, modelled after Subscriber and
  /// Subscription contracts while omitting flow control related signals.
  ///
  /// By omitting flow control between stream and channel automatons we greatly
  /// simplify implementation and move buffering into the connection automaton.

  /// A signal carrying serialized frame on the stream.
  ///
  /// This signal corresponds to Subscriber::onNext.
  void onNextFrame(Payload frame);

  /// Indicates a terminal signal from the connection.
  ///
  /// This signal corresponds to Subscriber::{onComplete,onError} and
  /// Subscription::cancel.
  /// Per ReactiveStreams specification:
  /// 1. no other signal can be delivered during or after this one,
  /// 2. "unsubscribe handshake" guarantees that the signal will be delivered
  ///   exactly once, even if the automaton initiated stream closure,
  /// 3. per "unsubscribe handshake", the automaton must deliver corresponding
  ///   terminal signal to the connection.
  virtual void endStream(StreamCompletionSignal signal) = 0;
  /// @}

 protected:
  /// @{
  /// ::onNextFrame(Payload) forwards to one of these functions depending on
  /// actual frame type. It is guaranteed that ::onNextFrame(Payload) does not
  /// access `this` after calling into one of the handlers below. This
  /// assumption is important for memory management.
  virtual void onNextFrame(Frame_REQUEST_SUB& frame) = 0;
  virtual void onNextFrame(Frame_REQUEST_CHANNEL& frame) = 0;
  virtual void onNextFrame(Frame_REQUEST_N& frame) = 0;
  virtual void onNextFrame(Frame_CANCEL& frame) = 0;
  virtual void onNextFrame(Frame_RESPONSE& frame) = 0;
  virtual void onNextFrame(Frame_ERROR& frame) = 0;

  /// Invoked when ::onNextFrame(Payload) failed to deserialize or was
  /// malformed.
  virtual void onBadFrame() = 0;
  /// @}

 private:
  /// Deserializes and dispatches frame according to template frame type
  /// parameter.
  template <typename Frame>
  void deserializeAndDispatch(Payload& paylaod);
};
}

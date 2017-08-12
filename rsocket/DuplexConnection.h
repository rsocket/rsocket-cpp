// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

#include "yarpl/flowable/Subscriber.h"

namespace folly {
class IOBuf;
}

namespace rsocket {

/// Represents a connection of the underlying protocol, on top of which the
/// RSocket protocol is layered.  The underlying protocol MUST provide an
/// ordered, guaranteed, bidirectional transport of frames.  Moreover, frame
/// boundaries MUST be preserved.
///
/// The frames exchanged through this interface are serialized, and lack the
/// optional frame length field.  Presence of the field is determined by the
/// underlying protocol.  If the protocol natively supports framing
/// (e.g. Aeron), the fileld MUST be omitted, otherwise (e.g. TCP) it must be
/// present.  The RSocket implementation MUST NOT be provided with a frame that
/// contains the length field nor can it ever send such a frame.
///
/// It can be assumed that both input and output will be closed by sending
/// appropriate terminal signals (according to ReactiveStreams specification)
/// before the connection is destroyed.
class DuplexConnection {
 public:
  using Subscriber = yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>;

  virtual ~DuplexConnection() = default;

  /// Sets a Subscriber that will consume received frames (a reader).
  ///
  /// If setInput() has already been called, then calling setInput() again will
  /// complete the previous subscriber.
  virtual void setInput(yarpl::Reference<Subscriber>) = 0;

  /// Obtains a Subscriber that should be fed with frames to send (a writer).
  ///
  /// If getOutput() has already been called, it is only safe to call again if
  /// all previous output subscribers have been terminated.
  virtual yarpl::Reference<Subscriber> getOutput() = 0;

  /// Whether the duplex connection respects frame boundaries.
  virtual bool isFramed() const {
    return false;
  }
};
}

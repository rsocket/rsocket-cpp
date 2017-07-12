// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include "yarpl/flowable/Subscriber.h"

namespace folly {
class IOBuf;
}

namespace rsocket {

/// Represents a connection of the underlying protocol, on top of which
/// the ReactiveSocket is layered. The underlying protocol MUST provide an
/// ordered, guaranteed, bidirectional transport of frames. Moreover, the frame
/// boundaries MUST be preserved.
///
/// The frames exchanged through this interface are serialized, and lack the
/// optional frame length field. Presence of the field is determined by the
/// underlying protocol. If the protocol natively supports framing (e.g. Aeron),
/// the fill MUST be omitted, otherwise (e.g. TCP) is must be present.
/// ReactiveSocket implementation MUST NOT ever be provided with a frame that
/// contains the length field nor it ever requests to sends such a frame.
///
/// It can be assumed that both input and output will be closed by sending
/// appropriate terminal signals (according to ReactiveStreams specification)
/// before the connection is destroyed.
class DuplexConnection {
 public:
  virtual ~DuplexConnection() = default;

  /// Sets a Subscriber that will consume received frames (a reader).
  ///
  /// This method is invoked by ReactiveSocket implementation once in an entire
  /// lifetime of the connection. The connection MUST NOT assume an ownership of
  /// provided Subscriber.
  virtual void setInput(
      yarpl::Reference<yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>>
          framesSink) = 0;

  /// Obtains a Subscriber that should be fed with frames to send (a writer).
  ///
  /// This method is invoked by ReactiveSocket
  /// implementation once in an entire lifetime of the connection. The
  /// connection MUST manage the lifetime of provided Subscriber.
  virtual yarpl::Reference<yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>>
  getOutput() = 0;

  /// property telling whether the duplex connection respects frame boundaries
  virtual bool isFramed() {
    return false;
  }
};
}

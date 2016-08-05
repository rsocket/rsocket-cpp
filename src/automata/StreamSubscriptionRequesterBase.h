// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include <reactive-streams/utilities/AllowanceSemaphore.h>
#include <reactive-streams/utilities/SmartPointers.h>
#include "src/AbstractStreamAutomaton.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/mixins/ConsumerMixin.h"
#include "src/mixins/ExecutorMixin.h"
#include "src/mixins/LoggingMixin.h"
#include "src/mixins/MemoryMixin.h"
#include "src/mixins/MixinTerminator.h"
#include "src/mixins/SourceIfMixin.h"
#include "src/mixins/StreamIfMixin.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

enum class StreamCompletionSignal;

/// Implementation of stream automaton that represents a Subscription requester.
class StreamSubscriptionRequesterBase
    : public LoggingMixin<ConsumerMixin<Frame_RESPONSE, MixinTerminator>> {
  using Base = LoggingMixin<ConsumerMixin<Frame_RESPONSE, MixinTerminator>>;

 public:
  using Base::Base;

  /// Degenerate form of the Subscriber interface -- only one request payload
  /// will be sent to the server.
  void onNext(Payload);

  /// @{
  /// A Subscriber interface to control ingestion of response payloads.
  void request(size_t);

  void cancel();
  /// @}

 protected:
  /// Override in subclass to send the correct type of request frame
  virtual void sendRequestFrame(FrameFlags, size_t, Payload&&) = 0;

  /// @{
  void endStream(StreamCompletionSignal);

  /// Not all frames are intercepted, some just pass through.
  using Base::onNextFrame;

  void onNextFrame(Frame_RESPONSE&);

  void onNextFrame(Frame_ERROR&);

  std::ostream& logPrefix(std::ostream& os);
  /// @}

  /// State of the Subscription requester.
  enum class State : uint8_t {
    NEW,
    REQUESTED,
    CLOSED,
  } state_{State::NEW};

  /// An allowance accumulated before the stream is initialised.
  /// Remaining part of the allowance is forwarded to the ConsumerMixin.
  reactivestreams::AllowanceSemaphore initialResponseAllowance_;
};
}

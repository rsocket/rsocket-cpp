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
#include "src/mixins/PublisherMixin.h"
#include "src/mixins/SinkIfMixin.h"
#include "src/mixins/SourceIfMixin.h"
#include "src/mixins/StreamIfMixin.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

enum class StreamCompletionSignal;

/// Implementation of stream automaton that represents a Channel requester.
class ChannelRequesterBase
    : public PublisherMixin<
          Frame_REQUEST_CHANNEL,
          ConsumerMixin<Frame_RESPONSE, MixinTerminator>> {
  using Base = PublisherMixin<
      Frame_REQUEST_CHANNEL,
      ConsumerMixin<Frame_RESPONSE, MixinTerminator>>;

 public:
  using Base::Base;

  /// @{
  /// A Subscriber implementation exposed to the user of ReactiveSocket to
  /// receive "request" payloads.
  void onSubscribe(Subscription&);

  void onNext(Payload);

  void onComplete();

  void onError(folly::exception_wrapper);
  /// @}

  /// @{
  void request(size_t);

  void cancel();
  /// @}

 protected:
  /// @{
  void endStream(StreamCompletionSignal);

  /// Not all frames are intercepted, some just pass through.
  using Base::onNextFrame;

  void onNextFrame(Frame_RESPONSE&&);

  void onNextFrame(Frame_ERROR&&);

  std::ostream& logPrefix(std::ostream& os);
  /// @}

 private:
  /// State of the Channel requester.
  enum class State : uint8_t {
    NEW,
    REQUESTED,
    CLOSED,
  } state_{State::NEW};
  /// An allowance accumulated before the stream is initialised.
  /// Remaining part of the allowance is forwarded to the ConsumerMixin.
  reactivestreams::AllowanceSemaphore initialResponseAllowance_;
};

using ChannelRequester = SourceIfMixin<SinkIfMixin<StreamIfMixin<
    ExecutorMixin<MemoryMixin<LoggingMixin<ChannelRequesterBase>>>>>>;
}

// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "reactivesocket-cpp/src/streams/AllowanceSemaphore.h"
#include "reactivesocket-cpp/src/streams/SmartPointers.h"
#include "reactivesocket-cpp/src/AbstractStreamAutomaton.h"
#include "reactivesocket-cpp/src/Frame.h"
#include "reactivesocket-cpp/src/Payload.h"
#include "reactivesocket-cpp/src/ReactiveStreamsCompat.h"
#include "reactivesocket-cpp/src/mixins/ConsumerMixin.h"
#include "reactivesocket-cpp/src/mixins/ExecutorMixin.h"
#include "reactivesocket-cpp/src/mixins/LoggingMixin.h"
#include "reactivesocket-cpp/src/mixins/MemoryMixin.h"
#include "reactivesocket-cpp/src/mixins/MixinTerminator.h"
#include "reactivesocket-cpp/src/mixins/PublisherMixin.h"
#include "reactivesocket-cpp/src/mixins/SinkIfMixin.h"
#include "reactivesocket-cpp/src/mixins/SourceIfMixin.h"
#include "reactivesocket-cpp/src/mixins/StreamIfMixin.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

enum class StreamCompletionSignal;

/// Implementation of stream automaton that represents a Channel requester.
class ChannelRequesterBase
    : public LoggingMixin<PublisherMixin<
          Frame_REQUEST_CHANNEL,
          LoggingMixin<ConsumerMixin<Frame_RESPONSE, MixinTerminator>>>> {
  using Base = LoggingMixin<PublisherMixin<
      Frame_REQUEST_CHANNEL,
      LoggingMixin<ConsumerMixin<Frame_RESPONSE, MixinTerminator>>>>;

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

  void onNextFrame(Frame_RESPONSE&);

  void onNextFrame(Frame_ERROR&);

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

using ChannelRequester =
    SourceIfMixin<SinkIfMixin<StreamIfMixin<LoggingMixin<ExecutorMixin<
        LoggingMixin<MemoryMixin<LoggingMixin<ChannelRequesterBase>>>>>>>>;
}

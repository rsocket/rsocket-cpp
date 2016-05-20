// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "reactive-streams-cpp/utilities/AllowanceSemaphore.h"
#include "reactive-streams-cpp/utilities/SmartPointers.h"
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

/// Implementation of stream automaton that represents a Channel responder.
class ChannelResponderBase
    : public LoggingMixin<PublisherMixin<
          Frame_RESPONSE,
          LoggingMixin<
              ConsumerMixin<Frame_REQUEST_CHANNEL, MixinTerminator>>>> {
  using Base = LoggingMixin<PublisherMixin<
      Frame_RESPONSE,
      LoggingMixin<ConsumerMixin<Frame_REQUEST_CHANNEL, MixinTerminator>>>>;

 public:
  using Base::Base;

  /// @{
  /// A Subscriber implementation exposed to the user of ReactiveSocket to
  /// receive "response" payloads.
  void onNext(Payload);

  void onComplete();

  void onError(folly::exception_wrapper);
  /// @}

  /// @{
  void request(size_t n);

  void cancel();
  /// @}

 protected:
  /// @{
  void endStream(StreamCompletionSignal);

  /// Not all frames are intercepted, some just pass through.
  using Base::onNextFrame;

  void onNextFrame(Frame_REQUEST_CHANNEL&);

  void onNextFrame(Frame_CANCEL&);

  std::ostream& logPrefix(std::ostream& os);
  /// @}

 private:
  /// State of the Channel responder.
  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  } state_{State::RESPONDING};
};

using ChannelResponder =
    SourceIfMixin<SinkIfMixin<StreamIfMixin<LoggingMixin<ExecutorMixin<
        LoggingMixin<MemoryMixin<LoggingMixin<ChannelResponderBase>>>>>>>>;
}

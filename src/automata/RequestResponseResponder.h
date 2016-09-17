// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "reactive-streams/utilities/SmartPointers.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/mixins/ExecutorMixin.h"
#include "src/mixins/LoggingMixin.h"
#include "src/mixins/MemoryMixin.h"
#include "src/mixins/MixinTerminator.h"
#include "src/mixins/PublisherMixin.h"
#include "src/mixins/SinkIfMixin.h"
#include "src/mixins/StreamIfMixin.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

enum class StreamCompletionSignal;

/// Implementation of stream automaton that represents a RequestResponse
/// responder
class RequestResponseResponderBase
    : public LoggingMixin<PublisherMixin<Frame_RESPONSE, MixinTerminator>> {
  using Base = LoggingMixin<PublisherMixin<Frame_RESPONSE, MixinTerminator>>;

 public:
  using Base::Base;

  /// @{
  /// A Subscriber implementation exposed to the user of ReactiveSocket to
  /// receive "response" payloads.
  void onNext(Payload);

  void onComplete();

  void onError(folly::exception_wrapper);
  /// @}

 protected:
  /// @{
  void endStream(StreamCompletionSignal);

  /// Not all frames are intercepted, some just pass through.
  using Base::onNextFrame;

  void onNextFrame(Frame_CANCEL&&);

  std::ostream& logPrefix(std::ostream& os);
  /// @}

  /// State of the Subscription responder.
  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  } state_{State::RESPONDING};
};

using RequestResponseResponder =
    SinkIfMixin<StreamIfMixin<LoggingMixin<ExecutorMixin<LoggingMixin<
        MemoryMixin<LoggingMixin<RequestResponseResponderBase>>>>>>>;
}

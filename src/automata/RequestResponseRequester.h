// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Optional.h>
#include <iosfwd>

#include <reactive-streams/utilities/SmartPointers.h>
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
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

/// Implementation of stream automaton that represents a RequestResponse
/// requester
class RequestResponseRequesterBase : public MixinTerminator {
 public:
  using MixinTerminator::MixinTerminator;

  /// Degenerate form of the Subscriber interface -- only one request payload
  /// will be sent to the server.
  void onNext(Payload);

  /// @{
  void subscribe(Subscriber<Payload>& subscriber);
  void request(size_t);
  void cancel();
  /// @}

  std::ostream& logPrefix(std::ostream& os);

 protected:
  /// @{
  void endStream(StreamCompletionSignal signal);

  /// Not all frames are intercepted, some just pass through.
  using MixinTerminator::onNextFrame;

  void onNextFrame(Frame_RESPONSE&&);
  void onNextFrame(Frame_ERROR&&);

  void onError(folly::exception_wrapper ex);
  /// @}

  /// State of the Subscription requester.
  enum class State : uint8_t {
    NEW,
    REQUESTED,
    CLOSED,
  } state_{State::NEW};

  // Whether the Subscriber made the request(1) call and thus is
  // ready to accept the payload.
  bool waitingForPayload_{false};
  folly::Optional<Payload> payload_;

  /// A Subscriber that will consume payloads.
  /// This mixin is responsible for delivering a terminal signal to the
  /// Subscriber once the stream ends.
  reactivestreams::SubscriberPtr<Subscriber<Payload>> consumingSubscriber_;
};

using RequestResponseRequester = SourceIfMixin<StreamIfMixin<
    ExecutorMixin<MemoryMixin<LoggingMixin<RequestResponseRequesterBase>>>>>;
}

// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include "rsocket/internal/Allowance.h"
#include "rsocket/statemachine/ConsumerBase.h"

namespace folly {
class exception_wrapper;
}

namespace rsocket {

enum class StreamCompletionSignal;

/// Implementation of stream stateMachine that represents a Stream requester
class StreamRequester : public ConsumerBase {
  using Base = ConsumerBase;

 public:
  // initialization of the ExecutorBase will be ignored for any of the
  // derived classes
  StreamRequester(
      std::shared_ptr<StreamsWriter> writer,
      StreamId streamId,
      Payload payload)
      : ConsumerBase(std::move(writer), streamId),
        initialPayload_(std::move(payload)) {}

  void setRequested(size_t n);

 private:
  // implementation from ConsumerBase::Subscription
  void request(int64_t) noexcept override;
  void cancel() noexcept override;

  void handlePayload(Payload&& payload, bool complete, bool flagsNext) override;
  void handleError(folly::exception_wrapper errorPayload) override;

  /// An allowance accumulated before the stream is initialised.
  /// Remaining part of the allowance is forwarded to the ConsumerBase.
  Allowance initialResponseAllowance_;

  /// Initial payload which has to be sent with 1st request.
  Payload initialPayload_;
  bool requested_{false};
};
}

// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include "src/automata/StreamSubscriptionRequesterBase.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

enum class StreamCompletionSignal;

/// Implementation of stream automaton that represents a Stream requester
class StreamRequester : public StreamSubscriptionRequesterBase {
  using Base = StreamSubscriptionRequesterBase;

 public:
  explicit StreamRequester(const Base::Parameters& params)
      : ExecutorBase(params.executor, false), Base(params) {}

  std::ostream& logPrefix(std::ostream& os);

 private:
  void sendRequestFrame(FrameFlags, size_t, Payload&&) override;
};
} // reactivesocket

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
  using Base::Base;

  std::ostream& logPrefix(std::ostream& os);

 private:
  void sendRequestFrame(FrameFlags, size_t, Payload&&) override;
};
} // reactivesocket

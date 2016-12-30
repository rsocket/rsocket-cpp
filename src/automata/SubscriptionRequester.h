// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include "src/automata/StreamSubscriptionRequesterBase.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Subscription requester.
class SubscriptionRequester : public StreamSubscriptionRequesterBase {
  using Base = StreamSubscriptionRequesterBase;

 public:
  explicit SubscriptionRequester(const Base::Parameters& params)
      : ExecutorBase(params.executor, false), Base(params) {}

  std::ostream& logPrefix(std::ostream& os);

 private:
  void sendRequestFrame(FrameFlags, size_t, Payload&&) override;
};

} // reactivesocket

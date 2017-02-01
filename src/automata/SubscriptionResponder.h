// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include "src/automata/StreamSubscriptionResponderBase.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Subscription responder.
class SubscriptionResponder : public StreamSubscriptionResponderBase {
  using Base = StreamSubscriptionResponderBase;

 public:
  explicit SubscriptionResponder(
      uint32_t initialRequestN,
      const Base::Parameters& params)
      : ExecutorBase(params.executor), Base(initialRequestN, params) {}

 private:
  void onCompleteImpl() noexcept override;
};

} // reactivesocket

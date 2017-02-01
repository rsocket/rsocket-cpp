// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include "src/automata/StreamSubscriptionResponderBase.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Stream responder
class StreamResponder : public StreamSubscriptionResponderBase {
  using Base = StreamSubscriptionResponderBase;

 public:
  explicit StreamResponder(
      uint32_t initialRequestN,
      const Base::Parameters& params)
      : ExecutorBase(params.executor), Base(initialRequestN, params) {}
};
} // reactivesocket

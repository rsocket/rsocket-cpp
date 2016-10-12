// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "src/automata/StreamSubscriptionResponderBase.h"
#include "src/mixins/ExecutorMixin.h"
#include "src/mixins/LoggingMixin.h"
#include "src/mixins/MemoryMixin.h"
#include "src/mixins/SinkIfMixin.h"
#include "src/mixins/StreamIfMixin.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Subscription responder.
class SubscriptionResponderBase : public StreamSubscriptionResponderBase {
  using Base = StreamSubscriptionResponderBase;

 public:
  using Base::Base;

 protected:
  std::ostream& logPrefix(std::ostream& os) {
    return os << "SubscriptionResponder(" << &connection_ << ", " << streamId_
              << "): ";
  }
};

using SubscriptionResponder = SinkIfMixin<StreamIfMixin<
    ExecutorMixin<MemoryMixin<LoggingMixin<SubscriptionResponderBase>>>>>;
}

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

/// Implementation of stream automaton that represents a Stream responder
class StreamResponderBase : public StreamSubscriptionResponderBase {
  using Base = StreamSubscriptionResponderBase;

 public:
  using Base::Base;

 protected:
  std::ostream& logPrefix(std::ostream& os) {
    return os << "StreamResponder(" << &connection_ << ", " << streamId_
              << "): ";
  }
};

using StreamResponder = SinkIfMixin<StreamIfMixin<LoggingMixin<ExecutorMixin<
    LoggingMixin<MemoryMixin<LoggingMixin<StreamResponderBase>>>>>>>;
}

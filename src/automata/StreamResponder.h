// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "reactive-streams/utilities/AllowanceSemaphore.h"
#include "reactive-streams/utilities/SmartPointers.h"
#include "src/AbstractStreamAutomaton.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/automata/StreamSubscriptionResponderBase.h"
#include "src/mixins/ExecutorMixin.h"
#include "src/mixins/LoggingMixin.h"
#include "src/mixins/MemoryMixin.h"
#include "src/mixins/SinkIfMixin.h"
#include "src/mixins/StreamIfMixin.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

enum class StreamCompletionSignal;

/// Implementation of stream automaton that represents a Stream responder
class StreamResponderBase : public StreamSubscriptionResponderBase {
  using Base = StreamSubscriptionResponderBase;

 public:
  using Base::Base;

 protected:
  /// @{

  /// Not all frames are intercepted, some just pass through.
  using Base::onNextFrame;

  void onNextFrame(Frame_REQUEST_STREAM&);

  std::ostream& logPrefix(std::ostream& os);
  /// @}
};

using StreamResponder = SinkIfMixin<StreamIfMixin<LoggingMixin<ExecutorMixin<
    LoggingMixin<MemoryMixin<LoggingMixin<StreamResponderBase>>>>>>>;
}

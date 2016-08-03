// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include <reactive-streams/utilities/AllowanceSemaphore.h>
#include <reactive-streams/utilities/SmartPointers.h>
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/automata/StreamSubscriptionRequesterBase.h"
#include "src/mixins/ConsumerMixin.h"
#include "src/mixins/ExecutorMixin.h"
#include "src/mixins/LoggingMixin.h"
#include "src/mixins/MemoryMixin.h"
#include "src/mixins/SourceIfMixin.h"
#include "src/mixins/StreamIfMixin.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

enum class StreamCompletionSignal;

/// Implementation of stream automaton that represents a Stream requester
class StreamRequesterBase
    : public StreamSubscriptionRequesterBase<Frame_REQUEST_STREAM> {
  using Base = StreamSubscriptionRequesterBase<Frame_REQUEST_STREAM>;

 public:
  using Base::Base;

 protected:
  /// @{
  std::ostream& logPrefix(std::ostream& os);
  /// @}
};

using StreamRequester = SourceIfMixin<StreamIfMixin<LoggingMixin<ExecutorMixin<
    LoggingMixin<MemoryMixin<LoggingMixin<StreamRequesterBase>>>>>>>;
}

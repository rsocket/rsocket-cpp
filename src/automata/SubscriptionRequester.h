// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include <reactive-streams/utilities/AllowanceSemaphore.h>
#include <reactive-streams/utilities/SmartPointers.h>
#include "src/AbstractStreamAutomaton.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/automata/StreamSubscriptionRequesterBase.h"
#include "src/mixins/ExecutorMixin.h"
#include "src/mixins/LoggingMixin.h"
#include "src/mixins/MemoryMixin.h"
#include "src/mixins/MixinTerminator.h"
#include "src/mixins/SourceIfMixin.h"
#include "src/mixins/StreamIfMixin.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

enum class StreamCompletionSignal;

/// Implementation of stream automaton that represents a Subscription requester.
class SubscriptionRequesterBase : public StreamSubscriptionRequesterBase {
  using Base = StreamSubscriptionRequesterBase;

 public:
  using Base::Base;

  std::ostream& logPrefix(std::ostream& os);

 protected:
  /// @{
  void sendRequestFrame(FrameFlags, size_t, Payload&&) override;
  /// @}
};

using SubscriptionRequester = SourceIfMixin<StreamIfMixin<
    ExecutorMixin<MemoryMixin<LoggingMixin<SubscriptionRequesterBase>>>>>;
}

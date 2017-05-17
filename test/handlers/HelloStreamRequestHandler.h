// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include "src/Payload.h"
#include "src/RSocketResponder.h"
#include "src/internal/ReactiveStreamsCompat.h"
#include "src/temporary_home/NullRequestHandler.h"
#include "src/temporary_home/SubscriptionBase.h"
#include "yarpl/Flowable.h"

namespace rsocket {
namespace tests {

class HelloStreamRequestHandler : public RSocketResponder {
 public:
  /// Handles a new inbound Stream requested by the other end.
  yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>
  handleRequestStream(rsocket::Payload request, rsocket::StreamId streamId)
      override;
};
}
}

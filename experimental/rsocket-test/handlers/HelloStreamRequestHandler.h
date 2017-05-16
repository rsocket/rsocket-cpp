// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include "rsocket/RSocketResponder.h"
#include "src/NullRequestHandler.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/SubscriptionBase.h"
#include "yarpl/Flowable.h"

namespace rsocket {
namespace tests {

class HelloStreamRequestHandler : public RSocketResponder {
 public:
  /// Handles a new inbound Stream requested by the other end.
  yarpl::Reference<yarpl::flowable::Flowable<reactivesocket::Payload>>
  handleRequestStream(
      reactivesocket::Payload request,
      reactivesocket::StreamId streamId) override;
};
}
}

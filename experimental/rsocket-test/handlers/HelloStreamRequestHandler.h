// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include "rsocket/RSocketRequestHandler.h"
#include "src/NullRequestHandler.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/SubscriptionBase.h"
#include "yarpl/v/Flowable.h"

namespace rsocket {
namespace tests {

class HelloStreamRequestHandler : public RSocketRequestHandler {
 public:
  /// Handles a new inbound Stream requested by the other end.
  yarpl::Reference<yarpl::Flowable<reactivesocket::Payload>>
  handleRequestStream(
      reactivesocket::Payload request,
      reactivesocket::StreamId streamId) override;
};
}
}

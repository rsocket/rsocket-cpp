// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include "src/NullRequestHandler.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/StandardReactiveSocket.h"
#include "src/SubscriptionBase.h"

class HelloStreamRequestHandler : public reactivesocket::DefaultRequestHandler {
 public:
  /// Handles a new inbound Stream requested by the other end.
  void handleRequestStream(
      reactivesocket::Payload request,
      reactivesocket::StreamId streamId,
      const std::shared_ptr<
          reactivesocket::Subscriber<reactivesocket::Payload>>&
          response) noexcept override;

  std::shared_ptr<reactivesocket::StreamState> handleSetupPayload(
      reactivesocket::ReactiveSocket&,
      reactivesocket::ConnectionSetupPayload request) noexcept override;
};

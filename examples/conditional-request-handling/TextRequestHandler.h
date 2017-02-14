// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include "src/StandardReactiveSocket.h"
#include "src/SubscriptionBase.h"
#include "src/NullRequestHandler.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

using namespace ::reactivesocket;

class TextRequestHandler : public DefaultRequestHandler {
public:
    /// Handles a new inbound Stream requested by the other end.
    void handleRequestStream(
            Payload request,
            StreamId streamId,
            const std::shared_ptr<Subscriber<Payload>>& response) noexcept override;

    std::shared_ptr<StreamState> handleSetupPayload(
            ReactiveSocket&,
            ConnectionSetupPayload request) noexcept override;
};
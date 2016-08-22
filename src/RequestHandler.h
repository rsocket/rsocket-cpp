// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

class RequestHandler {
 public:
  virtual ~RequestHandler() = default;

  /// Handles a new Channel requested by the other end.
  ///
  /// Modelled after Publisher::subscribe, hence must synchronously call
  /// Subscriber::onSubscribe, and provide a valid Subscription.
  virtual Subscriber<Payload>& handleRequestChannel(
      Payload request,
      Subscriber<Payload>& response) = 0;

  /// Handles a new Stream requested by the other end.
  virtual void handleRequestStream(
      Payload request,
      Subscriber<Payload>& response) = 0;

  /// Handles a new inbound Subscription requested by the other end.
  virtual void handleRequestSubscription(
      Payload request,
      Subscriber<Payload>& response) = 0;

  /// Handles a new fire-and-forget request sent by the other end.
  virtual void handleFireAndForgetRequest(Payload request) = 0;

  /// Handles a new metadata-push sent by the other end.
  virtual void handleMetadataPush(std::unique_ptr<folly::IOBuf> request) = 0;
};
}

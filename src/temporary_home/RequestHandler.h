// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/Payload.h"
#include "src/RSocketParameters.h"
#include "src/internal/Common.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/flowable/Subscription.h"

namespace rsocket {

class StreamState;
class ReactiveSocket;

class RequestHandler {
 public:
  virtual ~RequestHandler() = default;

  /// Handles a new Channel requested by the other end.
  virtual yarpl::Reference<yarpl::flowable::Subscriber<Payload>>
  handleRequestChannel(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          response) noexcept = 0;

  /// Handles a new Stream requested by the other end.
  virtual void handleRequestStream(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          response) noexcept = 0;

  /// Handles a new inbound RequestResponse requested by the other end.
  virtual void handleRequestResponse(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          response) noexcept = 0;

  /// Handles a new fire-and-forget request sent by the other end.
  virtual void handleFireAndForgetRequest(
      Payload request,
      StreamId streamId) noexcept = 0;

  /// Handles a new metadata-push sent by the other end.
  virtual void handleMetadataPush(
      std::unique_ptr<folly::IOBuf> request) noexcept = 0;


  // TODO: cleanup the methods above
  virtual void onSubscriptionPaused(
      const yarpl::Reference<yarpl::flowable::Subscription>&
          subscription) noexcept = 0;
  virtual void onSubscriptionResumed(
      const yarpl::Reference<yarpl::flowable::Subscription>&
          subscription) noexcept = 0;
  virtual void onSubscriberPaused(
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          subscriber) noexcept = 0;
  virtual void onSubscriberResumed(
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          subscriber) noexcept = 0;

  // TODO (T17774014): Move to separate interface
  virtual void socketOnConnected() {}
  virtual void socketOnDisconnected(folly::exception_wrapper& listener) {}
  virtual void socketOnClosed(folly::exception_wrapper& listener) {}
};
}

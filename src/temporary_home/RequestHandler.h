// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/internal/Common.h"
#include "src/temporary_home/ConnectionSetupPayload.h"
#include "src/Payload.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/flowable/Subscription.h"

namespace reactivesocket {

class StreamState;
class ReactiveSocket;

class RequestHandler {
 public:
  virtual ~RequestHandler() = default;

  /// Handles a new Channel requested by the other end.
  virtual yarpl::Reference<yarpl::flowable::Subscriber<Payload>> handleRequestChannel(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>& response) noexcept = 0;

  /// Handles a new Stream requested by the other end.
  virtual void handleRequestStream(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>& response) noexcept = 0;

  /// Handles a new inbound RequestResponse requested by the other end.
  virtual void handleRequestResponse(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>& response) noexcept = 0;

  /// Handles a new fire-and-forget request sent by the other end.
  virtual void handleFireAndForgetRequest(
      Payload request,
      StreamId streamId) noexcept = 0;

  /// Handles a new metadata-push sent by the other end.
  virtual void handleMetadataPush(
      std::unique_ptr<folly::IOBuf> request) noexcept = 0;

  /// Temporary home - this should eventually be an input to asking for a
  /// RequestHandler so negotiation is possible
  virtual std::shared_ptr<StreamState> handleSetupPayload(ConnectionSetupPayload request) noexcept = 0;

  /// Temporary home - this should accompany handleSetupPayload
  /// Return stream state for the given token. Return nullptr to disable resume
  virtual bool handleResume(
      ResumeParameters resumeParams) noexcept = 0;

  // Handle a stream that can resume in a "clean" state. Client and Server are
  // up-to-date.
  virtual void handleCleanResume(
      yarpl::Reference<yarpl::flowable::Subscription> response) noexcept = 0;

  // Handle a stream that can resume in a "dirty" state. Client is "behind"
  // Server.
  virtual void handleDirtyResume(
      yarpl::Reference<yarpl::flowable::Subscription> response) noexcept = 0;

  // TODO: cleanup the methods above
  virtual void onSubscriptionPaused(
      const yarpl::Reference<yarpl::flowable::Subscription>& subscription) noexcept = 0;
  virtual void onSubscriptionResumed(
      const yarpl::Reference<yarpl::flowable::Subscription>& subscription) noexcept = 0;
  virtual void onSubscriberPaused(
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>& subscriber) noexcept = 0;
  virtual void onSubscriberResumed(
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>& subscriber) noexcept = 0;

  // TODO (T17774014): Move to separate interface
  virtual void socketOnConnected(){}
  virtual void socketOnDisconnected(folly::exception_wrapper& listener){}
  virtual void socketOnClosed(folly::exception_wrapper& listener){}
};
}

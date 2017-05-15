// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/Common.h"
#include "src/ConnectionSetupPayload.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

class StreamState;
class ReactiveSocket;

class RequestHandler {
 public:
  virtual ~RequestHandler() = default;

  /// Handles a new Channel requested by the other end.
  virtual std::shared_ptr<Subscriber<Payload>> handleRequestChannel(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) noexcept = 0;

  /// Handles a new Stream requested by the other end.
  virtual void handleRequestStream(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) noexcept = 0;

  /// Handles a new inbound RequestResponse requested by the other end.
  virtual void handleRequestResponse(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) noexcept = 0;

  /// Handles a new fire-and-forget request sent by the other end.
  virtual void handleFireAndForgetRequest(
      Payload request,
      StreamId streamId) noexcept = 0;

  /// Handles a new metadata-push sent by the other end.
  virtual void handleMetadataPush(
      std::unique_ptr<folly::IOBuf> request) noexcept = 0;

  /// Temporary home - this should eventually be an input to asking for a
  /// RequestHandler so negotiation is possible
  virtual std::shared_ptr<StreamState> handleSetupPayload(
      ReactiveSocket& socket,
      ConnectionSetupPayload request) noexcept = 0;

  /// Temporary home - this should accompany handleSetupPayload
  /// Return stream state for the given token. Return nullptr to disable resume
  virtual bool handleResume(
      ReactiveSocket& socket,
      ResumeParameters resumeParams) noexcept = 0;

  // Handle a stream that can resume in a "clean" state. Client and Server are
  // up-to-date.
  virtual void handleCleanResume(
      std::shared_ptr<Subscription> response) noexcept = 0;

  // Handle a stream that can resume in a "dirty" state. Client is "behind"
  // Server.
  virtual void handleDirtyResume(
      std::shared_ptr<Subscription> response) noexcept = 0;

  // TODO: cleanup the methods above
  virtual void onSubscriptionPaused(
      const std::shared_ptr<Subscription>& subscription) noexcept = 0;
  virtual void onSubscriptionResumed(
      const std::shared_ptr<Subscription>& subscription) noexcept = 0;
  virtual void onSubscriberPaused(
      const std::shared_ptr<Subscriber<Payload>>& subscriber) noexcept = 0;
  virtual void onSubscriberResumed(
      const std::shared_ptr<Subscriber<Payload>>& subscriber) noexcept = 0;

  // TODO (T17774014): Move to separate interface
  virtual void socketOnConnected(){}
  virtual void socketOnDisconnected(folly::exception_wrapper& listener){}
  virtual void socketOnClosed(folly::exception_wrapper& listener){}
};
}

// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/Common.h"
#include "src/ConnectionSetupPayload.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace folly {
class Executor;
}

namespace reactivesocket {

class StreamState;

class SubscriberFactory {
 public:
  virtual ~SubscriberFactory() = default;
  virtual std::shared_ptr<Subscriber<Payload>> createSubscriber() = 0;
  virtual std::shared_ptr<Subscriber<Payload>> createSubscriber(
      folly::Executor& executor) = 0;
};

class RequestHandlerBase {
 public:
  virtual ~RequestHandlerBase() = default;

  //
  // client code uses subscriberFactory to create instances of subcsribers
  // there is an optional executor instance which can be passed to
  // SubscriberFactory::createSubscriber() method
  // client can ignore the subscriber factory if it desires to
  //

  /// Handles a new Channel requested by the other end.
  virtual std::shared_ptr<Subscriber<Payload>> onRequestChannel(
      Payload request,
      StreamId streamId,
      SubscriberFactory& subscriberFactory) = 0;

  /// Handles a new Stream requested by the other end.
  virtual void onRequestStream(
      Payload request,
      StreamId streamId,
      SubscriberFactory& subscriberFactory) = 0;

  /// Handles a new inbound Subscription requested by the other end.
  virtual void onRequestSubscription(
      Payload request,
      StreamId streamId,
      SubscriberFactory& subscriberFactory) = 0;

  /// Handles a new inbound RequestResponse requested by the other end.
  virtual void onRequestResponse(
      Payload request,
      StreamId streamId,
      SubscriberFactory& subscriberFactory) = 0;

  /// Handles a new fire-and-forget request sent by the other end.
  virtual void handleFireAndForgetRequest(
      Payload request,
      StreamId streamId) = 0;

  /// Handles a new metadata-push sent by the other end.
  virtual void handleMetadataPush(std::unique_ptr<folly::IOBuf> request) = 0;

  /// Temporary home - this should eventually be an input to asking for a
  /// RequestHandler so negotiation is possible
  virtual std::shared_ptr<StreamState> handleSetupPayload(
      ConnectionSetupPayload request) = 0;

  /// Temporary home - this should accompany handleSetupPayload
  /// Return stream state for the given token. Return nullptr to disable resume
  virtual std::shared_ptr<StreamState> handleResume(
      const ResumeIdentificationToken& token,
      ResumePosition position) = 0;

  // Handle a stream that can resume in a "clean" state. Client and Server are
  // up-to-date.
  virtual void handleCleanResume(std::shared_ptr<Subscription> response) = 0;

  // Handle a stream that can resume in a "dirty" state. Client is "behind"
  // Server.
  virtual void handleDirtyResume(std::shared_ptr<Subscription> response) = 0;
};

class RequestHandler : public RequestHandlerBase {
 public:
  //
  // Modelled after Publisher::subscribe, hence must synchronously call
  // Subscriber::onSubscribe, and provide a valid Subscription.
  //

  /// Handles a new Channel requested by the other end.
  virtual std::shared_ptr<Subscriber<Payload>> handleRequestChannel(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) = 0;

  /// Handles a new Stream requested by the other end.
  virtual void handleRequestStream(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) = 0;

  /// Handles a new inbound Subscription requested by the other end.
  virtual void handleRequestSubscription(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) = 0;

  /// Handles a new inbound RequestResponse requested by the other end.
  virtual void handleRequestResponse(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) = 0;

 private:
  std::shared_ptr<Subscriber<Payload>> onRequestChannel(
      Payload request,
      StreamId streamId,
      SubscriberFactory& subscriberFactory) override;

  /// Handles a new Stream requested by the other end.
  void onRequestStream(
      Payload request,
      StreamId streamId,
      SubscriberFactory& subscriberFactory) override;

  /// Handles a new inbound Subscription requested by the other end.
  void onRequestSubscription(
      Payload request,
      StreamId streamId,
      SubscriberFactory& subscriberFactory) override;

  /// Handles a new inbound RequestResponse requested by the other end.
  void onRequestResponse(
      Payload request,
      StreamId streamId,
      SubscriberFactory& subscriberFactory) override;
};
}

// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

#include <gmock/gmock.h>

#include "src/Payload.h"
#include "src/RequestHandler.h"

namespace reactivesocket {

class MockRequestHandlerBase : public RequestHandlerBase {
 public:
  MOCK_METHOD3(
      handleRequestChannel_,
      std::shared_ptr<Subscriber<Payload>>(
          Payload& request,
          StreamId streamId,
          SubscriberFactory&));
  MOCK_METHOD3(
      handleRequestStream_,
      void(Payload& request, StreamId streamId, SubscriberFactory&));
  MOCK_METHOD3(
      handleRequestSubscription_,
      void(Payload& request, StreamId streamId, SubscriberFactory&));
  MOCK_METHOD3(
      handleRequestResponse_,
      void(Payload& request, StreamId streamId, SubscriberFactory&));
  MOCK_METHOD2(
      handleFireAndForgetRequest_,
      void(Payload& request, StreamId streamId));
  MOCK_METHOD1(
      handleMetadataPush_,
      void(std::unique_ptr<folly::IOBuf>& request));
  MOCK_METHOD1(
      handleSetupPayload_,
      std::shared_ptr<StreamState>(ConnectionSetupPayload& request));
  MOCK_METHOD1(
      handleResume_,
      std::shared_ptr<StreamState>(const ResumeIdentificationToken& token));

  std::shared_ptr<Subscriber<Payload>> onRequestChannel(
      Payload request,
      StreamId streamId,
      SubscriberFactory& subscriberFactory) override {
    return handleRequestChannel_(request, streamId, subscriberFactory);
  }

  void onRequestStream(
      Payload request,
      StreamId streamId,
      SubscriberFactory& subscriberFactory) override {
    handleRequestStream_(request, streamId, subscriberFactory);
  }

  void onRequestSubscription(
      Payload request,
      StreamId streamId,
      SubscriberFactory& subscriberFactory) override {
    handleRequestSubscription_(request, streamId, subscriberFactory);
  }

  void onRequestResponse(
      Payload request,
      StreamId streamId,
      SubscriberFactory& subscriberFactory) override {
    handleRequestResponse_(request, streamId, subscriberFactory);
  }

  void handleFireAndForgetRequest(Payload request, StreamId streamId) override {
    handleFireAndForgetRequest_(request, streamId);
  }

  void handleMetadataPush(std::unique_ptr<folly::IOBuf> request) override {
    handleMetadataPush_(request);
  }

  std::shared_ptr<StreamState> handleSetupPayload(
      ConnectionSetupPayload request) override {
    return handleSetupPayload_(request);
  }

  std::shared_ptr<StreamState> handleResume(
      const ResumeIdentificationToken& token) override {
    return handleResume_(token);
  }

  void handleCleanResume(std::shared_ptr<Subscription> response) override {}
  void handleDirtyResume(std::shared_ptr<Subscription> response) override {}
};

class MockRequestHandler : public RequestHandler {
 public:
  MOCK_METHOD3(
      handleRequestChannel_,
      std::shared_ptr<Subscriber<Payload>>(
          Payload& request,
          StreamId streamId,
          const std::shared_ptr<Subscriber<Payload>>&));
  MOCK_METHOD3(
      handleRequestStream_,
      void(
          Payload& request,
          StreamId streamId,
          const std::shared_ptr<Subscriber<Payload>>&));
  MOCK_METHOD3(
      handleRequestSubscription_,
      void(
          Payload& request,
          StreamId streamId,
          const std::shared_ptr<Subscriber<Payload>>&));
  MOCK_METHOD3(
      handleRequestResponse_,
      void(
          Payload& request,
          StreamId streamId,
          const std::shared_ptr<Subscriber<Payload>>&));
  MOCK_METHOD2(
      handleFireAndForgetRequest_,
      void(Payload& request, StreamId streamId));
  MOCK_METHOD1(
      handleMetadataPush_,
      void(std::unique_ptr<folly::IOBuf>& request));
  MOCK_METHOD1(
      handleSetupPayload_,
      std::shared_ptr<StreamState>(ConnectionSetupPayload& request));
  MOCK_METHOD1(
      handleResume_,
      std::shared_ptr<StreamState>(const ResumeIdentificationToken& token));

  std::shared_ptr<Subscriber<Payload>> handleRequestChannel(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) override {
    return handleRequestChannel_(request, streamId, response);
  }

  void handleRequestStream(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) override {
    handleRequestStream_(request, streamId, response);
  }

  void handleRequestSubscription(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) override {
    handleRequestSubscription_(request, streamId, response);
  }

  void handleRequestResponse(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) override {
    handleRequestResponse_(request, streamId, response);
  }

  void handleFireAndForgetRequest(Payload request, StreamId streamId) override {
    handleFireAndForgetRequest_(request, streamId);
  }

  void handleMetadataPush(std::unique_ptr<folly::IOBuf> request) override {
    handleMetadataPush_(request);
  }

  std::shared_ptr<StreamState> handleSetupPayload(
      ConnectionSetupPayload request) override {
    return handleSetupPayload_(request);
  }

  std::shared_ptr<StreamState> handleResume(
      const ResumeIdentificationToken& token) override {
    return handleResume_(token);
  }

  void handleCleanResume(std::shared_ptr<Subscription> response) override {}
  void handleDirtyResume(std::shared_ptr<Subscription> response) override {}
};
}

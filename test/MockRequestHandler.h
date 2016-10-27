// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

#include <gmock/gmock.h>

#include "src/Payload.h"
#include "src/RequestHandler.h"

namespace reactivesocket {

class MockRequestHandlerBase : public RequestHandlerBase {
 public:
  MOCK_METHOD2(
      handleRequestChannel_,
      std::shared_ptr<Subscriber<Payload>>(
          Payload& request,
          SubscriberFactory&));
  MOCK_METHOD2(
      handleRequestStream_,
      void(Payload& request, SubscriberFactory&));
  MOCK_METHOD2(
      handleRequestSubscription_,
      void(Payload& request, SubscriberFactory&));
  MOCK_METHOD2(
      handleRequestResponse_,
      void(Payload& request, SubscriberFactory&));
  MOCK_METHOD1(handleFireAndForgetRequest_, void(Payload& request));
  MOCK_METHOD1(
      handleMetadataPush_,
      void(std::unique_ptr<folly::IOBuf>& request));
  MOCK_METHOD1(handleSetupPayload_, void(ConnectionSetupPayload& request));

  std::shared_ptr<Subscriber<Payload>> onRequestChannel(
      Payload request,
      SubscriberFactory& subscriberFactory) override {
    return handleRequestChannel_(request, subscriberFactory);
  }

  void onRequestStream(Payload request, SubscriberFactory& subscriberFactory)
      override {
    handleRequestStream_(request, subscriberFactory);
  }

  void onRequestSubscription(
      Payload request,
      SubscriberFactory& subscriberFactory) override {
    handleRequestSubscription_(request, subscriberFactory);
  }

  void onRequestResponse(Payload request, SubscriberFactory& subscriberFactory)
      override {
    handleRequestResponse_(request, subscriberFactory);
  }

  void handleFireAndForgetRequest(Payload request) override {
    handleFireAndForgetRequest_(request);
  }

  void handleMetadataPush(std::unique_ptr<folly::IOBuf> request) override {
    handleMetadataPush_(request);
  }

  void handleSetupPayload(ConnectionSetupPayload request) override {
    handleSetupPayload_(request);
  }
};

class MockRequestHandler : public RequestHandler {
 public:
  MOCK_METHOD2(
      handleRequestChannel_,
      std::shared_ptr<Subscriber<Payload>>(
          Payload& request,
          const std::shared_ptr<Subscriber<Payload>>&));
  MOCK_METHOD2(
      handleRequestStream_,
      void(Payload& request, const std::shared_ptr<Subscriber<Payload>>&));
  MOCK_METHOD2(
      handleRequestSubscription_,
      void(Payload& request, const std::shared_ptr<Subscriber<Payload>>&));
  MOCK_METHOD2(
      handleRequestResponse_,
      void(Payload& request, const std::shared_ptr<Subscriber<Payload>>&));
  MOCK_METHOD1(handleFireAndForgetRequest_, void(Payload& request));
  MOCK_METHOD1(
      handleMetadataPush_,
      void(std::unique_ptr<folly::IOBuf>& request));
  MOCK_METHOD1(handleSetupPayload_, void(ConnectionSetupPayload& request));

  std::shared_ptr<Subscriber<Payload>> handleRequestChannel(
      Payload request,
      const std::shared_ptr<Subscriber<Payload>>& response) override {
    return handleRequestChannel_(request, response);
  }

  void handleRequestStream(
      Payload request,
      const std::shared_ptr<Subscriber<Payload>>& response) override {
    handleRequestStream_(request, response);
  }

  void handleRequestSubscription(
      Payload request,
      const std::shared_ptr<Subscriber<Payload>>& response) override {
    handleRequestSubscription_(request, response);
  }

  void handleRequestResponse(
      Payload request,
      const std::shared_ptr<Subscriber<Payload>>& response) override {
    handleRequestResponse_(request, response);
  }

  void handleFireAndForgetRequest(Payload request) override {
    handleFireAndForgetRequest_(request);
  }

  void handleMetadataPush(std::unique_ptr<folly::IOBuf> request) override {
    handleMetadataPush_(request);
  }

  void handleSetupPayload(ConnectionSetupPayload request) override {
    handleSetupPayload_(request);
  }
};
}

// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

#include <gmock/gmock.h>

#include "src/Payload.h"
#include "src/RequestHandler.h"

namespace reactivesocket {

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
  MOCK_METHOD2(
      handleSetupPayload_,
      std::shared_ptr<StreamState>(
          ReactiveSocket& socket,
          ConnectionSetupPayload& request));
  MOCK_METHOD3(
      handleResume_,
      bool(
          ReactiveSocket& socket,
          const ResumeIdentificationToken& token,
          ResumePosition position));

  std::shared_ptr<Subscriber<Payload>> handleRequestChannel(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) noexcept override {
    return handleRequestChannel_(request, streamId, response);
  }

  void handleRequestStream(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) noexcept override {
    handleRequestStream_(request, streamId, response);
  }

  void handleRequestSubscription(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) noexcept override {
    handleRequestSubscription_(request, streamId, response);
  }

  void handleRequestResponse(
      Payload request,
      StreamId streamId,
      const std::shared_ptr<Subscriber<Payload>>& response) noexcept override {
    handleRequestResponse_(request, streamId, response);
  }

  void handleFireAndForgetRequest(
      Payload request,
      StreamId streamId) noexcept override {
    handleFireAndForgetRequest_(request, streamId);
  }

  void handleMetadataPush(
      std::unique_ptr<folly::IOBuf> request) noexcept override {
    handleMetadataPush_(request);
  }

  std::shared_ptr<StreamState> handleSetupPayload(
      ReactiveSocket& socket,
      ConnectionSetupPayload request) noexcept override {
    return handleSetupPayload_(socket, request);
  }

  bool handleResume(
      ReactiveSocket& socket,
      const ResumeIdentificationToken& token,
      ResumePosition position) noexcept override {
    return handleResume_(socket, token, position);
  }

  void handleCleanResume(
      std::shared_ptr<Subscription> response) noexcept override {}
  void handleDirtyResume(
      std::shared_ptr<Subscription> response) noexcept override {}

  void onSubscriptionPaused(
      const std::shared_ptr<Subscription>& subscription) noexcept override {}
  void onSubscriptionResumed(
      const std::shared_ptr<Subscription>& subscription) noexcept override {}
  void onSubscriberPaused(const std::shared_ptr<Subscriber<Payload>>&
                              subscriber) noexcept override {}
  void onSubscriberResumed(const std::shared_ptr<Subscriber<Payload>>&
                               subscriber) noexcept override {}
};
}

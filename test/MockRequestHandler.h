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
  MOCK_METHOD2(
      handleResume_,
      bool(ReactiveSocket& socket, ResumeParameters& resumeParams));

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
      ResumeParameters resumeParams) noexcept override {
    return handleResume_(socket, resumeParams);
  }

  void handleCleanResume(
      std::shared_ptr<Subscription> response) noexcept override {}
  void handleDirtyResume(
      std::shared_ptr<Subscription> response) noexcept override {}

  MOCK_METHOD1(
      onSubscriptionPaused_,
      void(const std::shared_ptr<Subscription>&));
  void onSubscriptionPaused(
      const std::shared_ptr<Subscription>& subscription) noexcept override {
    onSubscriptionPaused_(std::move(subscription));
  }
  void onSubscriptionResumed(
      const std::shared_ptr<Subscription>& subscription) noexcept override {}
  void onSubscriberPaused(const std::shared_ptr<Subscriber<Payload>>&
                              subscriber) noexcept override {}
  void onSubscriberResumed(const std::shared_ptr<Subscriber<Payload>>&
                               subscriber) noexcept override {}

  MOCK_METHOD0(socketOnConnected, void());

  MOCK_METHOD1(socketOnClosed, void(folly::exception_wrapper& listener));
  MOCK_METHOD1(socketOnDisconnected, void(folly::exception_wrapper& listener));
};
}

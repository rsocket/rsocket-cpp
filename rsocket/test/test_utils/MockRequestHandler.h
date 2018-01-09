// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

#include <gmock/gmock.h>

#include "rsocket/Payload.h"
#include "rsocket/temporary_home/RequestHandler.h"

namespace rsocket {

class MockRequestHandler : public RequestHandler {
 public:
  MOCK_METHOD3(
      handleRequestChannel_,
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>>(
          Payload& request,
          StreamId streamId,
          const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&));
  MOCK_METHOD3(
      handleRequestStream_,
      void(
          Payload& request,
          StreamId streamId,
          const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&));
  MOCK_METHOD3(
      handleRequestResponse_,
      void(
          Payload& request,
          StreamId streamId,
          const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&));
  MOCK_METHOD2(
      handleFireAndForgetRequest_,
      void(Payload& request, StreamId streamId));
  MOCK_METHOD1(
      handleMetadataPush_,
      void(std::unique_ptr<folly::IOBuf>& request));
  MOCK_METHOD1(
      handleSetupPayload_,
      std::shared_ptr<StreamState>(SetupParameters& request));
  MOCK_METHOD1(handleResume_, bool(ResumeParameters& resumeParams));

  yarpl::Reference<yarpl::flowable::Subscriber<Payload>> handleRequestChannel(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          response) noexcept override {
    return handleRequestChannel_(request, streamId, response);
  }

  void handleRequestStream(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          response) noexcept override {
    handleRequestStream_(request, streamId, response);
  }

  void handleRequestResponse(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          response) noexcept override {
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
      SetupParameters request) noexcept override {
    return handleSetupPayload_(request);
  }

  bool handleResume(ResumeParameters resumeParams) noexcept override {
    return handleResume_(resumeParams);
  }

  void handleCleanResume(yarpl::Reference<yarpl::flowable::Subscription>
                             response) noexcept override {}
  void handleDirtyResume(yarpl::Reference<yarpl::flowable::Subscription>
                             response) noexcept override {}

  MOCK_METHOD1(
      onSubscriptionPaused_,
      void(const yarpl::Reference<yarpl::flowable::Subscription>&));
  void onSubscriptionPaused(
      const yarpl::Reference<yarpl::flowable::Subscription>&
          subscription) noexcept override {
    onSubscriptionPaused_(std::move(subscription));
  }
  void onSubscriptionResumed(
      const yarpl::Reference<yarpl::flowable::Subscription>&
          subscription) noexcept override {}
  void onSubscriberPaused(
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          subscriber) noexcept override {}
  void onSubscriberResumed(
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>&
          subscriber) noexcept override {}

  MOCK_METHOD0(socketOnConnected, void());

  MOCK_METHOD1(socketOnClosed, void(folly::exception_wrapper& listener));
  MOCK_METHOD1(socketOnDisconnected, void(folly::exception_wrapper& listener));
};
}

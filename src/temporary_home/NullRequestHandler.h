// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include "src/temporary_home/ConnectionSetupPayload.h"
#include "RequestHandler.h"

namespace reactivesocket {

template <typename T>
class NullSubscriberT : public yarpl::flowable::Subscriber<T> {
 public:
  virtual ~NullSubscriberT() = default;

  // Subscriber methods
  void onSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> subscription) noexcept override {
    subscription->cancel();
  }
  void onNext(T element) noexcept override {}
  void onComplete() noexcept override {}
  void onError(const std::exception_ptr ex) noexcept override {}
};

extern template class NullSubscriberT<Payload>;
using NullSubscriber = NullSubscriberT<Payload>;

class NullSubscription : public yarpl::flowable::Subscription {
 public:
  // Subscription methods
  void request(int64_t n) noexcept override;
  void cancel() noexcept override;
};

class NullRequestHandler : public RequestHandler {
 public:
  yarpl::Reference<yarpl::flowable::Subscriber<Payload>> handleRequestChannel(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>& response) noexcept override;

  void handleRequestStream(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>& response) noexcept override;

  void handleRequestResponse(
      Payload request,
      StreamId streamId,
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>& response) noexcept override;

  void handleFireAndForgetRequest(
      Payload request,
      StreamId streamId) noexcept override;

  void handleMetadataPush(
      std::unique_ptr<folly::IOBuf> request) noexcept override;

  std::shared_ptr<StreamState> handleSetupPayload(
      ConnectionSetupPayload request) noexcept override;

  bool handleResume(ResumeParameters) noexcept override;

  void handleCleanResume(
      yarpl::Reference<yarpl::flowable::Subscription> response) noexcept override;
  void handleDirtyResume(
      yarpl::Reference<yarpl::flowable::Subscription> response) noexcept override;

  void onSubscriptionPaused(
      const yarpl::Reference<yarpl::flowable::Subscription>& subscription) noexcept override;
  void onSubscriptionResumed(
      const yarpl::Reference<yarpl::flowable::Subscription>& subscription) noexcept override;
  void onSubscriberPaused(
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>& subscriber) noexcept override;
  void onSubscriberResumed(
      const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>& subscriber) noexcept override;
};

using DefaultRequestHandler = NullRequestHandler;
}

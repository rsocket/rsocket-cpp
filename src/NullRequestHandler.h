// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/RequestHandler.h"

namespace reactivesocket {

class NullSubscriber : public Subscriber<Payload> {
 public:
  // Subscriber methods
  void onSubscribe(Subscription& subscription) override;
  void onNext(Payload element) override;
  void onComplete() override;
  void onError(folly::exception_wrapper ex) override;
};

class NullSubscription : public Subscription {
 public:
  // Subscription methods
  void request(size_t n) override;
  void cancel() override;
};

class NullRequestHandler : public RequestHandler {
 public:
  Subscriber<Payload>& handleRequestChannel(
      Payload request,
      Subscriber<Payload>& response) override;

  void handleRequestStream(Payload request, Subscriber<Payload>& response)
      override;

  void handleRequestSubscription(Payload request, Subscriber<Payload>& response)
      override;

  void handleFireAndForgetRequest(Payload request) override;

  void handleMetadataPush(Payload request) override;
};

using DefaultRequestHandler = NullRequestHandler;
}

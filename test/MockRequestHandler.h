// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>

#include <gmock/gmock.h>

#include "src/Payload.h"
#include "src/RequestHandler.h"

namespace reactivesocket {

class MockRequestHandler : public RequestHandler {
 public:
  MOCK_METHOD2(
      handleRequestChannel_,
      Subscriber<Payload>*(Payload& request, Subscriber<Payload>*));
  MOCK_METHOD2(
      handleRequestSubscription_,
      void(Payload& request, Subscriber<Payload>*));
  MOCK_METHOD1(
    handleFireAndForgetRequest_,
      void(Payload& request));

  Subscriber<Payload>& handleRequestChannel(
      Payload request,
      Subscriber<Payload>& response) override {
    return *handleRequestChannel_(request, &response);
  }

  void handleRequestSubscription(Payload request, Subscriber<Payload>& response)
      override {
    handleRequestSubscription_(request, &response);
  }

  void handleFireAndForgetRequest(Payload request) override {
    handleFireAndForgetRequest_(request);
  }
};
}

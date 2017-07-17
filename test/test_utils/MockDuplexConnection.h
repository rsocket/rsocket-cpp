// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <gmock/gmock.h>

#include "rsocket/DuplexConnection.h"
#include "test/test_utils/Mocks.h"

namespace rsocket {

class MockDuplexConnection : public DuplexConnection {
public:
  using Subscriber = yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>;

  MockDuplexConnection() {
    ON_CALL(*this, getOutput_()).WillByDefault(testing::Invoke([] {
      return yarpl::make_ref<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
    }));
  }

  void setInput(yarpl::Reference<Subscriber> in) override {
    setInput_(std::move(in));
  }

  yarpl::Reference<Subscriber> getOutput() override {
    return getOutput_();
  }

  MOCK_METHOD1(setInput_, void(yarpl::Reference<Subscriber>));
  MOCK_METHOD0(getOutput_, yarpl::Reference<Subscriber>());
};

}

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

  /// Creates a DuplexConnection that always runs `in` on the input
  /// subscriber and `out` on a default MockSubscriber.
  template <class InputFn, class OutputFn>
  MockDuplexConnection(InputFn in, OutputFn out) {
    EXPECT_CALL(*this, setInput_(testing::_))
        .WillRepeatedly(testing::Invoke(std::move(in)));
    EXPECT_CALL(*this, getOutput_())
        .WillRepeatedly(testing::Invoke([out = std::move(out)] {
          auto subscriber =
              yarpl::make_ref<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
          out(subscriber);
          return subscriber;
        }));
  }

  // DuplexConnection.

  void setInput(yarpl::Reference<Subscriber> in) override {
    setInput_(std::move(in));
  }

  yarpl::Reference<Subscriber> getOutput() override {
    return getOutput_();
  }

  // Mocks.

  MOCK_METHOD1(setInput_, void(yarpl::Reference<Subscriber>));
  MOCK_METHOD0(getOutput_, yarpl::Reference<Subscriber>());
};

}

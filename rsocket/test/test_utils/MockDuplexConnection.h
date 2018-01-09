// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <gmock/gmock.h>

#include "rsocket/DuplexConnection.h"
#include "yarpl/test_utils/Mocks.h"

namespace rsocket {

class MockDuplexConnection : public DuplexConnection {
 public:
  using Subscriber = DuplexConnection::Subscriber;

  MockDuplexConnection() {}

  /// Creates a DuplexConnection that always runs `in` on the input subscriber.
  template <class InputFn>
  MockDuplexConnection(InputFn in) {
    EXPECT_CALL(*this, setInput_(testing::_))
        .WillRepeatedly(testing::Invoke(std::move(in)));
  }

  // DuplexConnection.

  void setInput(yarpl::Reference<Subscriber> in) override {
    setInput_(std::move(in));
  }

  void send(std::unique_ptr<folly::IOBuf> buf) override {
    send_(buf);
  }

  // Mocks.

  MOCK_METHOD1(setInput_, void(yarpl::Reference<Subscriber>));
  MOCK_METHOD1(send_, void(std::unique_ptr<folly::IOBuf>&));
};

} // namespace rsocket

// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <gmock/gmock.h>

#include "src/DuplexConnection.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

class MockDuplexConnection : public DuplexConnection {
 public:
  MOCK_METHOD1(
      setInput_,
      void(Subscriber<std::unique_ptr<folly::IOBuf>>* framesSink));
  MOCK_METHOD0(getOutput_, Subscriber<std::unique_ptr<folly::IOBuf>>*());

  void setInput(
      Subscriber<std::unique_ptr<folly::IOBuf>>& framesSink) override {
    setInput_(&framesSink);
  }

  Subscriber<std::unique_ptr<folly::IOBuf>>& getOutput() override {
    return *getOutput_();
  }
};
}

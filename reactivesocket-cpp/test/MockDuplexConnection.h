// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <gmock/gmock.h>

#include "reactivesocket-cpp/src/DuplexConnection.h"
#include "reactivesocket-cpp/src/Payload.h"
#include "reactivesocket-cpp/src/ReactiveStreamsCompat.h"

namespace lithium {
namespace reactivesocket {

class MockDuplexConnection : public DuplexConnection {
 public:
  MOCK_METHOD1(setInput_, void(Subscriber<Payload>* framesSink));
  MOCK_METHOD0(getOutput_, Subscriber<Payload>*());

  void setInput(Subscriber<Payload>& framesSink) override {
    setInput_(&framesSink);
  }

  Subscriber<Payload>& getOutput() override {
    return *getOutput_();
  }
};
}
}

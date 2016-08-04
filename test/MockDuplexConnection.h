// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <gmock/gmock.h>

#include "src/DuplexConnection.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

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

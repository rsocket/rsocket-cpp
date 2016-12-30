// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include "src/automata/StreamSubscriptionResponderBase.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Stream responder
class StreamResponder : public StreamSubscriptionResponderBase {
  using Base = StreamSubscriptionResponderBase;

 public:
  explicit StreamResponder(const Base::Parameters& params)
      : ExecutorBase(params.executor, false), Base(params) {}

  std::ostream& logPrefix(std::ostream& os) {
    return os << "StreamResponder(" << &connection_ << ", " << streamId_
              << "): ";
  }

  void processInitialFrame(Frame_REQUEST_STREAM&&);
};
} // reactivesocket

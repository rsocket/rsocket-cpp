// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include "src/automata/StreamSubscriptionResponderBase.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Stream responder
class StreamResponder : public StreamSubscriptionResponderBase {
  using Base = StreamSubscriptionResponderBase;

 public:
  using Base::Base;

  std::ostream& logPrefix(std::ostream& os) {
    return os << "StreamResponder(" << &connection_ << ", " << streamId_
              << "): ";
  }
};
} // reactivesocket

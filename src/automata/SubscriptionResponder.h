// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include "src/automata/StreamSubscriptionResponderBase.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Subscription responder.
class SubscriptionResponder : public StreamSubscriptionResponderBase {
  using Base = StreamSubscriptionResponderBase;

 public:
  using Base::Base;

  std::ostream& logPrefix(std::ostream& os) {
    return os << "SubscriptionResponder(" << &connection_ << ", " << streamId_
              << "): ";
  }

  void onCompleteImpl() override {
    LOG(FATAL) << "onComplete is not allowed on Subscription iteraction.";
  }
};

} // reactivesocket

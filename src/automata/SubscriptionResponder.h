// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include "src/automata/StreamSubscriptionResponderBase.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Subscription responder.
class SubscriptionResponder : public StreamSubscriptionResponderBase {
  using Base = StreamSubscriptionResponderBase;

 public:
  explicit SubscriptionResponder(const Base::Parameters& params)
      : ExecutorBase(params.executor, false), Base(params) {}

  void processInitialFrame(Frame_REQUEST_SUB&&);

  std::ostream& logPrefix(std::ostream& os);

 private:
  void onCompleteImpl() override;
};

} // reactivesocket

// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <reactive-streams/utilities/SmartPointers.h>
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

class CancellingSubscription : public Subscription {
 public:
  explicit CancellingSubscription(
      std::shared_ptr<Subscriber<Payload>> subscriber)
      : subscriber_(std::move(subscriber)) {}

  // Subscription methods
  void request(size_t n) override {
    subscriber_.onComplete();
  }

  void cancel() override {
    subscriber_.onComplete();
  }

 private:
  SubscriberPtr<Subscriber<Payload>> subscriber_;
};

} // reactivesocket

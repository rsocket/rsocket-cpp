// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <reactive-streams/utilities/SmartPointers.h>
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

class FramedWriter : public reactivesocket::Subscriber<Payload>,
                     public reactivesocket::Subscription {
 public:
  explicit FramedWriter(reactivesocket::Subscriber<Payload>& stream)
      : stream_(&stream) {}

  // Subscriber methods
  void onSubscribe(reactivesocket::Subscription& subscription) override;
  void onNext(reactivesocket::Payload element) override;
  void onComplete() override;
  void onError(folly::exception_wrapper ex) override;

  // Subscription methods
  void request(size_t n) override;
  void cancel() override;

 private:
  SubscriberPtr<reactivesocket::Subscriber<Payload>> stream_;
  SubscriptionPtr<::reactivestreams::Subscription> writerSubscription_;
};

} // reactive socket

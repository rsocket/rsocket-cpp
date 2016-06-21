// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <reactivesocket-cpp/src/ReactiveStreamsCompat.h>
#include "reactivesocket-cpp/src/Payload.h"
#include "reactivesocket-cpp/src/mixins/IntrusiveDeleter.h"

namespace reactivesocket {
class PrintSubscriber : public IntrusiveDeleter, public Subscriber<Payload> {
 public:
  ~PrintSubscriber() override = default;

  void onSubscribe(Subscription& subscription) override;

  void onNext(Payload element) override;

  void onComplete() override;

  void onError(folly::exception_wrapper ex) override;
};
}

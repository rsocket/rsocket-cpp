// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {
class PrintSubscriber : public Subscriber<Payload> {
 public:
  ~PrintSubscriber();

  void onSubscribe(std::shared_ptr<Subscription> subscription) override;

  void onNext(Payload element) override;

  void onComplete() override;

  void onError(folly::exception_wrapper ex) override;
};
}

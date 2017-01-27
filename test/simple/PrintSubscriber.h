// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {
class PrintSubscriber : public Subscriber<Payload> {
 public:
  ~PrintSubscriber();

  void onSubscribe(
      std::shared_ptr<Subscription> subscription) noexcept override;
  void onNext(Payload element) noexcept override;
  void onComplete() noexcept override;
  void onError(folly::exception_wrapper ex) noexcept override;
};
}

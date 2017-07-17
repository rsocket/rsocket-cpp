// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/Payload.h"
#include "yarpl/flowable/Subscriber.h"

namespace rsocket {
class PrintSubscriber : public yarpl::flowable::Subscriber<Payload> {
 public:
  ~PrintSubscriber();

  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>
                       subscription) noexcept override;
  void onNext(Payload element) noexcept override;
  void onComplete() noexcept override;
  void onError(std::exception_ptr ex) noexcept override;
};
}

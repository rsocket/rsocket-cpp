// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "tck-test/BaseSubscriber.h"

#include "yarpl/Flowable.h"

namespace rsocket {
namespace tck {

class FlowableSubscriber : public BaseSubscriber,
                           public yarpl::flowable::Subscriber<Payload> {
 public:
  explicit FlowableSubscriber(int initialRequestN = 0);

  // Inherited from BaseSubscriber
  void request(int n) override;
  void cancel() override;

 protected:
  // Inherited from flowable::Subscriber
  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>
                       subscription) noexcept override;
  void onNext(Payload element) noexcept override;
  void onComplete() noexcept override;
  void onError(std::exception_ptr ex) noexcept override;

 private:
  yarpl::Reference<yarpl::flowable::Subscription> subscription_;
  int initialRequestN_{0};
};

} // tck
} // reactivesocket

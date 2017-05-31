// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "tck-test/BaseSubscriber.h"

#include "yarpl/Single.h"

namespace rsocket {
namespace tck {

class SingleSubscriber : public BaseSubscriber,
                         public yarpl::single::SingleObserver<Payload> {
 public:
  // Inherited from BaseSubscriber
  void request(int n) override;
  void cancel() override;

 protected:
  // Inherited from flowable::Subscriber
  void onSubscribe(yarpl::Reference<yarpl::single::SingleSubscription>
                       subscription) noexcept override;
  void onSuccess(Payload element) noexcept override;
  void onError(std::exception_ptr ex) noexcept override;

 private:
  yarpl::Reference<yarpl::single::SingleSubscription> subscription_;
};

} // tck
} // reactivesocket

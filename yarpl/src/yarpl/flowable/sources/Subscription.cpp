// Copyright 2004-present Facebook. All Rights Reserved.

#include "yarpl/flowable/Subscription.h"

namespace yarpl {
namespace flowable {

yarpl::Reference<Subscription> Subscription::empty() {
  class NullSubscription : public Subscription {
    void request(int64_t) override {}
    void cancel() override {}
  };
  return make_ref<NullSubscription>();
}

} // flowable
} // yarpl

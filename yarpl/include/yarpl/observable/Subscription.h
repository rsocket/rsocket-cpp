// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <vector>
#include <folly/Synchronized.h>
#include "yarpl/Refcounted.h"

namespace yarpl {
namespace observable {

class Subscription : public virtual Refcounted {
 public:
  virtual ~Subscription() = default;
  virtual void cancel();
  bool isCancelled() const;

  // Adds ability to tie another subscription to this instance.
  // Whenever *this subscription is cancelled then all tied subscriptions get
  // cancelled as well
  void tieSubscription(Reference<Subscription> subscription);

 protected:
  std::atomic<bool> cancelled_{false};
  folly::Synchronized<std::vector<Reference<Subscription>>> tiedSubscriptions_;
};

} // observable
} // yarpl

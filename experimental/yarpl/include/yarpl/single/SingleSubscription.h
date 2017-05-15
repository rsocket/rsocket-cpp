#pragma once

#include "../Refcounted.h"

namespace yarpl {
namespace single {

class SingleSubscription : public virtual Refcounted {
 public:
  virtual ~SingleSubscription() = default;
  virtual void cancel() = 0;

 protected:
  SingleSubscription() {}
};

} // single
} // yarpl
